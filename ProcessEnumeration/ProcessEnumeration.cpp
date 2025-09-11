#include <windows.h>
#include <comdef.h>
#include <tlhelp32.h>
#include <Wbemidl.h>
#include <psapi.h>
#include <iostream>
#include <format>

#include "logManagement.h"


#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wbemuuid.lib")

#pragma comment(lib, "Psapi.lib")

using std::vector;
using std::string;
using std::bit_cast;
using std::cout;
using std::format;

// Simple RAII privilege toggler for SeDebugPrivilege
class SeDebugScope {
public:
	explicit SeDebugScope(bool enable) : _enabled(false), _token(nullptr) {
		
		if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &_token))
		{
			return;
		}

		LUID luid{};
		
		if (!LookupPrivilegeValueW(nullptr, SE_DEBUG_NAME, &luid))
			return;

		TOKEN_PRIVILEGES tp{};
		tp.PrivilegeCount = 1;
		tp.Privileges[0].Luid = luid;
		tp.Privileges[0].Attributes = enable ? SE_PRIVILEGE_ENABLED : 0;

		// We save tthe previous state to be able to restore in necessity
		DWORD prevSize = 0;
		AdjustTokenPrivileges(_token, FALSE, &tp, 0, nullptr, nullptr);
		
		if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)
		{
			// Means the caller is not elevated or privilege not held.
			return;
		}

		_enabled = true;
		_luid = luid;
	}

	~SeDebugScope() {
	
		if (_enabled && _token)
		{
			TOKEN_PRIVILEGES tp{};
			tp.PrivilegeCount = 1;
			tp.Privileges[0].Luid = _luid;
			tp.Privileges[0].Attributes = 0; // disabled
			AdjustTokenPrivileges(_token, FALSE, &tp, 0, nullptr, nullptr);
		}
		
		if (_token) CloseHandle(_token);
	}

	bool enabled() const { return _enabled; }

private:
	bool _enabled;
	HANDLE _token;
	LUID _luid{};
};


VOID EnumProcesses_low_privs()
{
	string Msg = {};

	DWORD proc_ids[1024];
	DWORD ret_bytes;

	DWORD bResult;
	bResult = EnumProcesses(proc_ids, sizeof(proc_ids), &ret_bytes);
	
	if (!bResult)
	{
		printf("Can not execute EnumProcesses()...\n");
		return;
	}

	DWORD dwCount = ret_bytes / sizeof(DWORD);
	printf("%u working processes found..\n", dwCount);

	for (DWORD i = 0; i < dwCount; i++) {
		
		HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, proc_ids[i]);
		
		if (hProc)
		{
			wchar_t lpExeName[MAX_PATH];
			DWORD lpdwSize = MAX_PATH;
			bResult = QueryFullProcessImageNameW(hProc, 0, lpExeName, &lpdwSize);
			
			if (!bResult)
			{
				printf("Error from QueryFullProcessImageNameW");
			}
			else 
			{
				
				Msg = format("%s", lpExeName); // error, Msg = string and lpExeName = wchar_t
				WriteLog(Msg);
			}
		}
	}
}


// Trying to list process with SEDEBUG privilege enabled
VOID EnumProcesses_debug_privs()
{
	SeDebugScope seDebug(true);

	if (!seDebug.enabled())
	{
		printf("Warning: could not enable SeDebugPrivilege (run as admin?)\n");
		return; // We could continue anyway, as OpenProcess may still works for some PIDs, but we do not.
	}

	DWORD proc_ids[1024];
	DWORD ret_bytes = 0;

	if (!EnumProcesses(proc_ids, sizeof(proc_ids), &ret_bytes))
	{
		printf("Can not execute EnumProcesses()...\n");
		return;
	}

	DWORD dwCount = ret_bytes / sizeof(DWORD);
	printf("%u working processes found..\n", dwCount);

	for (DWORD i = 0; i < dwCount; i++) {
	
		if (proc_ids[i] == 0) continue;

		// We try to open the process with full access as we should have SeDebugPrivilege
		HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, proc_ids[i]);
		
		if (!hProc) {
			// Error with opening the process with PROCESS_ALL_ACCESS, so we try to fall back to limited.
			hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, proc_ids[i]);
		}

		if (hProc) {
			wchar_t pathBuf[MAX_PATH];
			DWORD len = MAX_PATH;
		
			if (QueryFullProcessImageNameW(hProc, 0, pathBuf, &len)) {
				wprintf(L"%5lu  %ls\n", proc_ids[i], pathBuf);
			}
			else {
				wprintf(L"%5lu  <unknown path> (QueryFullProcessImageNameW failed: %lu)\n",
					proc_ids[i], GetLastError());
			}
			
			CloseHandle(hProc);
		}
		else {
			wprintf(L"%5lu  <could not open> (OpenProcess error: %lu)\n",
				proc_ids[i], GetLastError());
		}
	}
}


VOID EnumProcesses_ToolHelpSnapShot32()
{
	HANDLE hProcessSnapshot;
	PROCESSENTRY32 pe32;

	hProcessSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if (hProcessSnapshot == INVALID_HANDLE_VALUE)
	{
		printf("Error taking snapshot : " + GetLastError());
		return;
	}

	// Set the size of the structure before using it.
	pe32.dwSize = sizeof(PROCESSENTRY32);

	if (!Process32First(hProcessSnapshot, &pe32))
	{
		printf("Error in taking information about first process : " + GetLastError());
		return;
	}

	do
	{
		printf("-----------------------------------\n");
		wprintf(L"%-20.19s\n", pe32.szExeFile);
		printf("-----------------------------------\n");
		wprintf(L"Proces ID : %9d\n", pe32.th32ProcessID);
		wprintf(L"Parent Process ID : %9d\n", pe32.th32ParentProcessID);
		wprintf(L"Thread count : %d\n\n", pe32.cntThreads);

		// Possibility to Enum modules and thread associated with processes

	} while (Process32NextW(hProcessSnapshot, &pe32));

	CloseHandle(hProcessSnapshot);
	return;
}



int main()
{
    std::cout << "Hello World!\n";
	EnumProcesses_low_privs();
	//EnumProcesses_debug_privs();
	//EnumProcesses_ToolHelpSnapShot32();
}

