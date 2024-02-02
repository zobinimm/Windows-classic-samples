#include "stdafx.h"
#include "ShellIconOverlay.h"

IFACEMETHODIMP ShellIconOverlay::GetOverlayInfo(LPWSTR pwszIconFile, int cchMax, int* pIndex, DWORD* pdwFlags)
{
    wcscpy_s(pwszIconFile, cchMax, L"D:\\download\\test.ico");
    *pIndex = 0;
    *pdwFlags = ISIOI_ICONFILE | ISIOI_ICONINDEX;
    return S_OK;
}

IFACEMETHODIMP ShellIconOverlay::GetPriority(int* pPriority)
{
    *pPriority = 0;
    return S_OK;
}

IFACEMETHODIMP ShellIconOverlay::IsMemberOf(LPCWSTR pwszPath, DWORD dwAttrib)
{
    return S_OK;
}
