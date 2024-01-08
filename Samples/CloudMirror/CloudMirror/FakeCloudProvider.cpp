// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include "stdafx.h"

//===============================================================
// FakeCloudProvider
//
//   This is the top-level class that implements our fake
//   cloud provider. It's the entry point ("Start") and
//   the different facets of implementing a cloud provider
//   are implemented in a bunch of helper classes. This
//   helps keep the intent of each class crisp for easier
//   digestion.
//
// Fakery Factor:
//
//   Most of it is usable as-is. You would want to avoid using
//   the FileCopierWithProgress class and replace that with
//   your own code that brings stuff down from a real cloud
//   server and stores it on the client.
//
//   And the shutdown story will be different.
//
//===============================================================

// This key is used so that the table of callbacks below can be
// registered/unregistered
CF_CONNECTION_KEY FakeCloudProvider::s_transferCallbackConnectionKey;

// This is a list of callbacks our fake provider support. This
// class has the callback methods, which are then delegated to
// helper classes
CF_CALLBACK_REGISTRATION FakeCloudProvider::s_MirrorCallbackTable[] =
{
    { CF_CALLBACK_TYPE_FETCH_DATA, FakeCloudProvider::OnFetchData },
    { CF_CALLBACK_TYPE_CANCEL_FETCH_DATA, FakeCloudProvider::OnCancelFetchData },
    { CF_CALLBACK_TYPE_NOTIFY_DELETE, FakeCloudProvider::OnFileOrFolderDelete },
    CF_CALLBACK_REGISTRATION_END
};

// Starts the Fake Cloud Provider. Returns when you press CTRL-C in the console window.
bool FakeCloudProvider::Start(_In_opt_ LPCWSTR serverFolder, _In_opt_ LPCWSTR clientFolder)
{
    auto result{ false };

    if (ProviderFolderLocations::Init(serverFolder, clientFolder))
    {
        // Stage 1: Setup
        //--------------------------------------------------------------------------------------------
        // The client folder (syncroot) must be indexed in order for states to properly display
        Utilities::AddFolderToSearchIndexer(ProviderFolderLocations::GetClientFolder());
        // Start up the task that registers and hosts the services for the shell (such as custom states, menus, etc)
        ShellServices::InitAndStartServiceTask();
        // Register the provider with the shell so that the Sync Root shows up in File Explorer
        CloudProviderRegistrar::RegisterWithShell();
        // Hook up callback methods (in this class) for transferring files between client and server
        ConnectSyncRootTransferCallbacks();
        // Create the placeholders in the client folder so the user sees something
        Placeholders::Create(ProviderFolderLocations::GetServerFolder(), L"", ProviderFolderLocations::GetClientFolder());

        // Stage 2: Running
        //--------------------------------------------------------------------------------------------
        // The file watcher loop for this sample will run until the user presses Ctrl-C.
        // The file watcher will look for any changes on the files in the client (syncroot) in order
        // to let the cloud know.
        CloudProviderSyncRootWatcher::WatchAndWait();

        // Stage 3: Done Running-- caused by CTRL-C
        //--------------------------------------------------------------------------------------------
        // Unhook up those callback methods
        DisconnectSyncRootTransferCallbacks();

        //  A real sync engine should NOT unregister the sync root upon exit.
        //  This is just to demonstrate the use of StorageProviderSyncRootManager::Unregister.
        CloudProviderRegistrar::Unregister();

        // And if we got here, then this was a normally run test versus crash-o-rama
        result = true;
    }

    return result;
}

// When the client needs to fetch data from the cloud, this method will be called.
// The FakeMirrorDataMover class does the actual work of copying files from
// the "cloud" to the "client" and updating the transfer status along the way.
void CALLBACK FakeCloudProvider::OnFetchData(
    _In_ CONST CF_CALLBACK_INFO* callbackInfo,
    _In_ CONST CF_CALLBACK_PARAMETERS* callbackParameters)
{
    FileCopierWithProgress::CopyFromServerToClient(callbackInfo, callbackParameters, ProviderFolderLocations::GetServerFolder());
}

// When the fetch is cancelled, this happens. Our FakeMirrorDataMover doesn't really care, because
// it's fake. 
void CALLBACK FakeCloudProvider::OnCancelFetchData(
    _In_ CONST CF_CALLBACK_INFO* callbackInfo,
    _In_ CONST CF_CALLBACK_PARAMETERS* callbackParameters)
{
    FileCopierWithProgress::CancelCopyFromServerToClient(callbackInfo, callbackParameters);
}

void CALLBACK FakeCloudProvider::OnFileOrFolderDelete(
    _In_ CONST CF_CALLBACK_INFO* callbackInfo,
    _In_ CONST CF_CALLBACK_PARAMETERS* callbackParameters)
{
    auto now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    std::tm localTime;
    localtime_s(&localTime, &currentTime);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;


    std::wstring fullClientPath(callbackInfo->VolumeDosName);
    fullClientPath.append(callbackInfo->NormalizedPath);

    //wprintf(L"%04d-%02d-%02d %02d:%02d:%02d.%03lld Path = %s\n", localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday,
    //    localTime.tm_hour, localTime.tm_min, localTime.tm_sec, ms.count(), fullClientPath.c_str());

    CF_OPERATION_INFO opInfo = { 0 };
    opInfo.StructSize = sizeof(CF_OPERATION_INFO);
    opInfo.Type = CF_OPERATION_TYPE_ACK_DELETE;
    opInfo.ConnectionKey = callbackInfo->ConnectionKey;
    opInfo.TransferKey = callbackInfo->TransferKey;
    opInfo.CorrelationVector = callbackInfo->CorrelationVector;
    opInfo.RequestKey = callbackInfo->RequestKey;
    CF_OPERATION_PARAMETERS params = { 0 };
    params.ParamSize = sizeof(CF_OPERATION_PARAMETERS);
    params.AckDelete.Flags = CF_OPERATION_ACK_DELETE_FLAG_NONE;

    // I have also tested many other error codes.
    params.AckDelete.CompletionStatus = STATUS_SUCCESS;

    DeleteAction deleteAction;
    deleteAction.OpInfo = opInfo;
    deleteAction.Parameters = params;
    deleteAction.FullPath = fullClientPath;

    std::thread myThread(std::bind(OnNotifyDeleteAction, deleteAction));
    myThread.detach();

    //HRESULT res = CfExecute(&opInfo, &params);
    //if (res != 0)
    //{
    //    wprintf(L"%04d-%02d-%02d %02d:%02d:%02d.%03lld CfExecute Error\n", localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday,
    //        localTime.tm_hour, localTime.tm_min, localTime.tm_sec, ms.count());
    //}

    now = std::chrono::system_clock::now();
    currentTime = std::chrono::system_clock::to_time_t(now);
    localtime_s(&localTime, &currentTime);
    ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    //wprintf(L"%04d-%02d-%02d %02d:%02d:%02d.%03lld OnFileOrFolderDelete end\n", localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday,
    //    localTime.tm_hour, localTime.tm_min, localTime.tm_sec, ms.count());
    return;
}

void FakeCloudProvider::OnNotifyDeleteAction(DeleteAction& deleteAction)
{
    std::wstring fullPath = deleteAction.FullPath;
    auto now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    std::tm localTime;
    localtime_s(&localTime, &currentTime);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    wprintf(L"%04d-%02d-%02d %02d:%02d:%02d.%03lld Path = %s\n", localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday,
        localTime.tm_hour, localTime.tm_min, localTime.tm_sec, ms.count(), fullPath.c_str());

    CF_OPERATION_INFO opInfo = deleteAction.OpInfo;
    CF_OPERATION_PARAMETERS opParams = deleteAction.Parameters;
    CfExecute(&opInfo, &opParams);
}

// Registers the callbacks in the table at the top of this file so that the methods above
// are called for our fake provider
void FakeCloudProvider::ConnectSyncRootTransferCallbacks()
{
    try
    {
        // Connect to the sync root using Cloud File API
        winrt::check_hresult(
            CfConnectSyncRoot(
                ProviderFolderLocations::GetClientFolder(),
                s_MirrorCallbackTable,
                NULL,
                CF_CONNECT_FLAG_REQUIRE_PROCESS_INFO |
                CF_CONNECT_FLAG_REQUIRE_FULL_FILE_PATH,
                &s_transferCallbackConnectionKey));
    }
    catch (...)
    {
        // winrt::to_hresult() will eat the exception if it is a result of winrt::check_hresult,
        // otherwise the exception will get rethrown and this method will crash out as it should
        wprintf(L"Could not connect to sync root, hr %08x\n", static_cast<HRESULT>(winrt::to_hresult()));
    }
}

// Unregisters the callbacks in the table at the top of this file so that 
// the client doesn't Hindenburg
void FakeCloudProvider::DisconnectSyncRootTransferCallbacks()
{
    wprintf(L"Shutting down\n");
    try
    {
        winrt::check_hresult(CfDisconnectSyncRoot(s_transferCallbackConnectionKey));
    }
    catch (...)
    {
        // winrt::to_hresult() will eat the exception if it is a result of winrt::check_hresult,
        // otherwise the exception will get rethrown and this method will crash out as it should
        wprintf(L"Could not disconnect the sync root, hr %08x\n", static_cast<HRESULT>(winrt::to_hresult()));
    }
}


