// Adapted upstream Win32 platform utilities: UTF-8 public paths and wide native dialogs.
#include "hzpch.h"
#include "Hazel/Utils/PlatformUtils.h"
#include "Hazel/Core/Application.h"
#include <commdlg.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <cstring>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace Hazel {
static std::wstring ToWide(const char* text, int length)
{
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, length, nullptr, 0);
    if (!count) return {};
    std::wstring result(count, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, length, result.data(), count);
    return result;
}
static std::string FileDialog(const char* filter, bool save)
{
    std::size_t length = 0;
    while (filter && filter[length]) {
        length += std::strlen(filter + length) + 1;
        length += std::strlen(filter + length) + 1;
    }
    auto nativeFilter = filter ? ToWide(filter, static_cast<int>(length + 1)) : std::wstring();
    std::wstring extension;
    if (filter && *filter) {
        const char* pattern = filter + std::strlen(filter) + 1;
        std::string firstPattern(pattern);
        firstPattern = firstPattern.substr(0, firstPattern.find(';'));
        if (firstPattern.rfind("*.", 0) == 0 && firstPattern.find('*', 1) == std::string::npos)
            extension = ToWide(firstPattern.data() + 2, static_cast<int>(firstPattern.size() - 2));
    }
    std::wstring filename(32768, L'\0');
    auto directory = std::filesystem::current_path().wstring();
    OPENFILENAMEW native{};
    native.lStructSize = sizeof(native);
    native.hwndOwner = glfwGetWin32Window(static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow()));
    native.lpstrFile = filename.data(); native.nMaxFile = static_cast<DWORD>(filename.size());
    native.lpstrInitialDir = directory.c_str();
    native.lpstrFilter = nativeFilter.empty() ? nullptr : nativeFilter.c_str();
    native.nFilterIndex = 1;
    native.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    native.lpstrDefExt = extension.empty() ? nullptr : extension.c_str();
    if (!(save ? GetSaveFileNameW(&native) : GetOpenFileNameW(&native))) return {};
    int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, filename.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (!count) return {};
    std::string result(count, '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, filename.c_str(), -1, result.data(), count, nullptr, nullptr);
    result.pop_back();
    return result;
}
std::string FileDialogs::SelectFolder() {
    auto initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) return {};
    IFileOpenDialog* dialog = nullptr;
    std::string result;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
        DWORD options = 0; dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
        dialog->SetTitle(L"Select folder");
        auto owner = glfwGetWin32Window(static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow()));
        if (SUCCEEDED(dialog->Show(owner))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                    result = std::filesystem::path(path).u8string(); CoTaskMemFree(path);
                }
                item->Release();
            }
        }
        dialog->Release();
    }
    if (SUCCEEDED(initialized)) CoUninitialize();
    return result;
}
bool FileDialogs::OpenPath(const std::string& path) {
    return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",std::filesystem::u8path(path).c_str(),nullptr,nullptr,SW_SHOWNORMAL))>32;
}
std::string FileDialogs::OpenFile(const char* filter) { return FileDialog(filter, false); }
std::string FileDialogs::SaveFile(const char* filter) { return FileDialog(filter, true); }
}
