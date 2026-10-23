#include "gazeora/host/launch/remote_thread_injector.h"

namespace gazeora {

namespace {

constexpr DWORD kLoadTimeoutMs = 10000;

std::wstring LastErrorString(const wchar_t* what) {
    return std::wstring(what) + L" failed (error " + std::to_wstring(GetLastError()) + L")";
}

}  // namespace

bool RemoteThreadInjector::Inject(HANDLE process, const std::wstring& dllPath,
                                  std::wstring& error) {
    size_t bytes = (dllPath.size() + 1) * sizeof(wchar_t);
    void* remotePath =
        VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remotePath) {
        error = LastErrorString(L"VirtualAllocEx");
        return false;
    }

    bool ok = false;
    bool remoteThreadRunning = false;
    if (!WriteProcessMemory(process, remotePath, dllPath.c_str(), bytes, nullptr)) {
        error = LastErrorString(L"WriteProcessMemory");
    } else {
        // kernel32 is mapped at the same address in every process of a boot session.
        auto loadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(
            GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
        HANDLE thread =
            CreateRemoteThread(process, nullptr, 0, loadLibrary, remotePath, 0, nullptr);
        if (!thread) {
            error = LastErrorString(L"CreateRemoteThread");
        } else {
            if (WaitForSingleObject(thread, kLoadTimeoutMs) != WAIT_OBJECT_0) {
                error = L"timed out waiting for LoadLibraryW in the target";
                remoteThreadRunning = true;
            } else {
                // Exit code is the low 32 bits of the HMODULE; 0 means LoadLibrary failed.
                DWORD exitCode = 0;
                GetExitCodeThread(thread, &exitCode);
                ok = exitCode != 0;
                if (!ok) {
                    error = L"LoadLibraryW failed inside the target (missing DLL dependency?)";
                }
            }
            CloseHandle(thread);
        }
    }

    // Leak the path rather than free it under a LoadLibraryW that may still be reading it.
    if (!remoteThreadRunning) {
        VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
    }
    return ok;
}

}  // namespace gazeora
