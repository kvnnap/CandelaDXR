#include "FileUtil.h"

#include "Window/WindowsDef.h"

#include <ShlDisp.h>
#include <atlbase.h>

bool candela::util::UnzipToFolder(const char* zipFileName, const char* outFolderName)
{
    // the shell object
    CComPtr<IShellDispatch> shell;
    HRESULT hr = shell.CoCreateInstance(CLSID_Shell);
    if (FAILED(hr))
        return false;

    // the zip file
    CComPtr<Folder> zipFile;
    hr = shell->NameSpace(CComVariant(zipFileName), &zipFile);
    if (FAILED(hr))
        return false;

    // destination folder
    CComPtr<Folder> destination;
    hr = shell->NameSpace(CComVariant(outFolderName), &destination);
    if (FAILED(hr))
        return false;

    // folder items inside the zip file
    CComPtr<FolderItems> folderItems;
    hr = zipFile->Items(&folderItems);
    if (FAILED(hr))
        return false;

    // copy operation - https://docs.microsoft.com/en-us/windows/win32/shell/folder-copyhere
    hr = destination->CopyHere(CComVariant(folderItems), CComVariant(2048 | 1024 | 512 | 16 | 4, VT_I4));

    return SUCCEEDED(hr);
}
