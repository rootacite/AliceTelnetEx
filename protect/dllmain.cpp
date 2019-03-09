// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "stdafx.h"

#include "detours.h"
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#pragma comment(lib,"detours.lib")

//DWORD MainId = 0;

void DebugPrivilege()
{
	HANDLE hToken = NULL;
	//打开当前进程的访问令牌
	int hRet = OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &hToken);
	if (hRet)
	{
		TOKEN_PRIVILEGES tp;
		tp.PrivilegeCount = 1;
		//取得描述权限的LUID
		LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tp.Privileges[0].Luid);
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		//调整访问令牌的权限
		AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
		CloseHandle(hToken);
	}
}

DWORD GetProcessIDByName(LPCWSTR pName)
{
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (INVALID_HANDLE_VALUE == hSnapshot) {
		return NULL;
	}
	PROCESSENTRY32 pe = { sizeof(pe) };
	for (BOOL ret = Process32First(hSnapshot, &pe); ret; ret = Process32Next(hSnapshot, &pe)) {
		if (lstrcmpW(pe.szExeFile, pName) == 0) {
			CloseHandle(hSnapshot);
			return pe.th32ProcessID;
		}
		//printf("%-6d %s\n", pe.th32ProcessID, pe.szExeFile);
	}
	CloseHandle(hSnapshot);
	return 0;
}


void InjectDLL(HANDLE hProcess, LPCWSTR dllFilePathName) {
	int cch = 1 + lstrlenW(dllFilePathName);
	int cb = cch * sizeof(wchar_t); 
	PWSTR PszLibFileRemote = (LPWSTR)VirtualAllocEx(hProcess, NULL, cb, MEM_COMMIT, PAGE_READWRITE);
	WriteProcessMemory(hProcess, PszLibFileRemote, (PVOID)dllFilePathName, cb, NULL);
	HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (PTHREAD_START_ROUTINE)LoadLibraryW, PszLibFileRemote, 0, NULL);
	WaitForSingleObject(hThread, INFINITE);
}

HANDLE
(WINAPI*
pOpenProcess)(
	 DWORD dwDesiredAccess,
	 BOOL bInheritHandle,
	 DWORD dwProcessId
) = OpenProcess;

HANDLE WINAPI HkOpenProcess(
	DWORD dwDesiredAccess,
	BOOL bInheritHandle,
	DWORD dwProcessId){
	DWORD MainId = GetProcessIDByName(L"AliceClient.exe");
	if (MainId == dwProcessId) {

		//MessageBoxA(0, "PREVENTED", "", 0);
		SetLastError(5);
		return 0;
	}
	return pOpenProcess(dwDesiredAccess,bInheritHandle,dwProcessId);
}

void hook() {
	DetourRestoreAfterWith();
	DetourTransactionBegin();

	DetourUpdateThread(GetCurrentThread());
	//DetourAttach((void**)&pCreateProcessA, HkCreateProcessA);
	//DetourAttach((void**)&pCreateProcessW, HkCreateProcessW);
	DetourAttach((void**)&pOpenProcess, HkOpenProcess);
	DetourTransactionCommit();
}

void unhook() {
	DetourRestoreAfterWith();
	DetourTransactionBegin();

	DetourUpdateThread(GetCurrentThread());
	//DetourDetach((void**)&pCreateProcessA, HkCreateProcessA);
	//DetourDetach((void**)&pCreateProcessW, HkCreateProcessW);
	DetourDetach((void**)&pOpenProcess, HkOpenProcess);
	DetourTransactionCommit();
}

HHOOK hcbt;
HMODULE hmod;
LRESULT CALLBACK CbtProc(int code,WPARAM wp,LPARAM lp) {
	return CallNextHookEx(hcbt, code, wp, lp);
}

extern "C" __declspec(dllexport)void starthook() {
	hcbt = SetWindowsHookEx(WH_CBT, CbtProc, hmod, 0);
}

extern "C" __declspec(dllexport)void cbunhook() {
	//hcbt = SetWindowsHookEx(WH_CBT, CbtProc, hmod, 0);
	UnhookWindowsHookEx(hcbt);
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		hook();
		hmod = hModule;
		break;
    case DLL_THREAD_ATTACH:
		break;
    case DLL_THREAD_DETACH:
		break;
    case DLL_PROCESS_DETACH:
		unhook();
        break;
    }
    return TRUE;
}

