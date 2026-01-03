#include "PlatformUtils.h"
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <iostream>
#include <algorithm>

namespace Utils {

    std::string GetWindowsFontsPath() {
        char path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_FONTS, NULL, 0, path))) {
            return std::string(path);
        }
        return "C:\\Windows\\Fonts";
    }

    void EnumerateFontsFromKey(HKEY hKeyRoot, const char* subKey, std::vector<FontInfo>& fonts, const std::string& winFontPath) {
        HKEY hKey;
        if (RegOpenKeyExA(hKeyRoot, subKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            char valueName[1024];
            char valueData[1024];
            DWORD nameLen, dataLen, type;
            DWORD index = 0;

            while (true) {
                nameLen = sizeof(valueName);
                dataLen = sizeof(valueData);
                LONG ret = RegEnumValueA(hKey, index, valueName, &nameLen, NULL, &type, (LPBYTE)valueData, &dataLen);
                
                if (ret == ERROR_NO_MORE_ITEMS) break;
                if (ret == ERROR_SUCCESS && type == REG_SZ) {
                    std::string fontName = valueName;
                    std::string fontFile = valueData;

                    // Remove " (TrueType)" etc from name
                    size_t parenPos = fontName.find(" (");
                    if (parenPos != std::string::npos) {
                        fontName = fontName.substr(0, parenPos);
                    }

                    // Check if path is absolute or relative to Fonts folder
                    std::string fullPath = fontFile;
                    if (fontFile.find(":\\") == std::string::npos) {
                        fullPath = winFontPath + "\\" + fontFile;
                    }

                    fonts.push_back({ fontName, fullPath });
                }
                index++;
            }
            RegCloseKey(hKey);
        }
    }

    std::vector<FontInfo> GetSystemFonts() {
        std::vector<FontInfo> fonts;
        std::string winFontPath = GetWindowsFontsPath();

        // Check Local Machine
        EnumerateFontsFromKey(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", fonts, winFontPath);
        
        // Check Current User (User installed fonts)
        EnumerateFontsFromKey(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", fonts, winFontPath);

        // Sort by name
        std::sort(fonts.begin(), fonts.end(), [](const FontInfo& a, const FontInfo& b) {
            return a.name < b.name;
        });

        return fonts;
    }

    std::string PickFolderDialog() {
        std::string result = "";
        
        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        if (SUCCEEDED(hr)) {
            IFileOpenDialog* pFileOpen;

            // Create the FileOpenDialog object.
            hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

            if (SUCCEEDED(hr)) {
                DWORD dwOptions;
                if (SUCCEEDED(pFileOpen->GetOptions(&dwOptions))) {
                    pFileOpen->SetOptions(dwOptions | FOS_PICKFOLDERS);
                }
                
                // Show the Open dialog box.
                hr = pFileOpen->Show(NULL);

                // Get the file name from the dialog box.
                if (SUCCEEDED(hr)) {
                    IShellItem* pItem;
                    hr = pFileOpen->GetResult(&pItem);
                    if (SUCCEEDED(hr)) {
                        PWSTR pszFilePath;
                        hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

                        if (SUCCEEDED(hr)) {
                            // Convert WCHAR to std::string
                            int len = WideCharToMultiByte(CP_UTF8, 0, pszFilePath, -1, NULL, 0, NULL, NULL);
                            if (len > 0) {
                                std::vector<char> buf(len);
                                WideCharToMultiByte(CP_UTF8, 0, pszFilePath, -1, &buf[0], len, NULL, NULL);
                                result = std::string(buf.data());
                            }
                            CoTaskMemFree(pszFilePath);
                        }
                        pItem->Release();
                    }
                }
                pFileOpen->Release();
            }
            CoUninitialize();
        }
        return result;
    }

    std::string PickFileDialog(const char* filter) {
        char szFile[260] = { 0 };
        OPENFILENAMEA ofn = { 0 };
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = filter ? filter : "All Files\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

        if (GetOpenFileNameA(&ofn)) {
            return std::string(ofn.lpstrFile);
        }
        return "";
    }

    std::string SaveFileDialog(const char* filter, const char* defaultName) {
        char szFile[260] = { 0 };
        if (defaultName) strncpy(szFile, defaultName, 259);

        OPENFILENAMEA ofn = { 0 };
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = filter ? filter : "All Files\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

        if (GetSaveFileNameA(&ofn)) {
            return std::string(ofn.lpstrFile);
        }
        return "";
    }

    std::string GetConfigDir() {
        char path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path))) {
            std::string p = std::string(path) + "\\FontExporter";
            CreateDirectoryA(p.c_str(), NULL);
            return p;
        }
        return ".";
    }
}
#endif
