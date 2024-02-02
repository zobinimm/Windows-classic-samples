#pragma once

class __declspec(uuid("D343F9C6-1740-432C-8A99-85D72C0D9569")) ShellIconOverlay : public winrt::implements<ShellIconOverlay, IShellIconOverlayIdentifier>
{
public:
    IFACEMETHODIMP    GetOverlayInfo(LPWSTR pwszIconFile, int cchMax, int* pIndex, DWORD* pdwFlags);
    IFACEMETHODIMP    GetPriority(int* pPriority);
    IFACEMETHODIMP    IsMemberOf(LPCWSTR pwszPath, DWORD dwAttrib);

private:
};

