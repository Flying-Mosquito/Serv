#pragma comment(lib, "ws2_32.lib")

#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <process.h>

#define BUFSIZE 1024

typedef struct //소켓정보 구조체
{
	SOCKET hClntSock;
	SOCKADDR_IN clntAddr;
} PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

typedef struct // 소켓버퍼정보 구조체
{
	OVERLAPPED overlapped;
	char buffer[BUFSIZE];
	WSABUF wsaBuf;
} PER_IO_DATA, *LPPER_IO_DATA;

typedef struct
{
	bool cnFlag;
	bool boost;
	bool vamp;
	WSABUF pVec;
} CLNT_INFO;

typedef struct
{
	bool flag;
	int count;
	SOCKADDR_IN addr[4];
	CLNT_INFO usr[4];
} ROOM_INFO;

ROOM_INFO gState[750];
int rCount;

unsigned int __stdcall CompletionThread(LPVOID pComPort);
void ErrorHandling(char *message);

int main(int argc, char** argv)
{
	WSADATA wsaData;
	HANDLE hCompletionPort;
	SYSTEM_INFO SystemInfo;
	SOCKADDR_IN servAddr;
	LPPER_IO_DATA PerIoData;
	LPPER_HANDLE_DATA PerHandleData;
	SOCKET hServSock;
	int RecvBytes;
	int i, Flags;
	int cnt = 0;

	rCount = 0;

	for (int u = 0; u < 750; u++)
	{
		gState[u].flag = false;
		gState[u].count = 0;
		for (int j = 0; j < 4; j++)
		{
			gState[u].usr[j].cnFlag = false;
			gState[u].usr[j].boost = false;
			gState[u].usr[j].vamp = false;
		}
	}

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		ErrorHandling("WSAStartup() error!");

	hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	GetSystemInfo(&SystemInfo);	// 시스템 정보를 얻음. CPU 개수만큼 쓰레드 만들기 위해 필요(dwNumberOfProcessors)

	// 쓰레드를 CPU 개수만큼 생성
	for (i = 0; i < SystemInfo.dwNumberOfProcessors; i++)
		_beginthreadex(NULL, 0, CompletionThread, (LPVOID)hCompletionPort, 0, NULL);

	hServSock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAddr.sin_port = htons(atoi("2738"));

	bind(hServSock, (SOCKADDR*)&servAddr, sizeof(servAddr));
	listen(hServSock, 5);

	while (TRUE)
	{
		SOCKET hClntSock;
		SOCKADDR_IN clntAddr;
		int addrLen = sizeof(clntAddr);
		
		hClntSock = accept(hServSock, (SOCKADDR*)&clntAddr, &addrLen);
		printf("[서버] 클라이언트 접속 : IP 주소 = %s, 포트 = %d\n",
			inet_ntoa(clntAddr.sin_addr), ntohs(clntAddr.sin_port));
		
		for (int u = 0; u < 750; u++)
		{
			if (gState[u].flag == false)
			{
				gState[u].flag = true;
				gState[u].usr[gState[u].count].cnFlag = true;
				gState[u].addr[gState[u].count] = clntAddr;
				gState[u].count++;
				printf("%d번방의 %d 번째 플레이어로 접속합니다.\n", (u+1), gState[u].count);
				rCount = u;
				break;
			}
			else if (gState[u].flag == true)
			{
				if (gState[u].count < 4)
				{
					gState[u].usr[gState[u].count].cnFlag = true;
					gState[u].addr[gState[u].count] = clntAddr;
					gState[u].count++;
					int tmp = gState[u].count + 1;
					send(hClntSock, (char*)&tmp, sizeof(int), 0);
					printf("%d번방의 %d 번째 플레이어로 접속합니다.\n", (u+1), gState[u].count);
					break;
				}
			}
		}

		// 연결된 클라의 소켓과 주소 정보 설정, PerHandleData = 클라정보
		PerHandleData = (LPPER_HANDLE_DATA)malloc(sizeof(PER_HANDLE_DATA));
		PerHandleData->hClntSock = hClntSock;
		memcpy(&(PerHandleData->clntAddr), &clntAddr, addrLen);

		// CP 연결(CompletionPort)
		CreateIoCompletionPort((HANDLE)hClntSock, hCompletionPort, (DWORD)PerHandleData, 0);

		// 다시 구조체 초기화
		PerIoData = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
		memset(&(PerIoData->overlapped), 0, sizeof(OVERLAPPED));
		PerIoData->wsaBuf.len = BUFSIZE;
		PerIoData->wsaBuf.buf = PerIoData->buffer;

		Flags = 0;

		WSARecv(PerHandleData->hClntSock, // 데이터 입력소켓.
			&(PerIoData->wsaBuf),  // 데이터 입력 버퍼포인터.
			1,       // 데이터 입력 버퍼의 수.
			(LPDWORD)&RecvBytes,
			(LPDWORD)&Flags,
			&(PerIoData->overlapped), // OVERLAPPED 구조체 포인터.
			NULL
			);
	}
	return 0;
}

//입출력 완료에 따른 쓰레드의 행동 정의
unsigned int __stdcall CompletionThread(LPVOID pComPort)
{
	HANDLE hCompletionPort = (HANDLE)pComPort;
	DWORD BytesTransferred;
	LPPER_HANDLE_DATA PerHandleData;
	LPPER_IO_DATA PerIoData;
	DWORD flags;

	while (1)
	{
		// IO 정보
		GetQueuedCompletionStatus(hCompletionPort,    // Completion Port
			&BytesTransferred,   // 전송된 바이트수
			(LPDWORD)&PerHandleData,
			(LPOVERLAPPED*)&PerIoData, // OVERLAPPED 구조체 포인터.
			INFINITE
			);

		// 연결 종료시
		if (BytesTransferred == 0) //EOF 전송시.
		{
			closesocket(PerHandleData->hClntSock);
			free(PerHandleData);
			free(PerIoData);
			continue;
		}

		PerIoData->wsaBuf.buf[BytesTransferred] = '\0';
		printf("Recv[%s]\n", PerIoData->wsaBuf.buf);

		for (int i = 0; i < (rCount+1); i++)
		{
			if (gState[i].flag == true)
			{
				for (int j = 0; j < 4; j++)
				{
					if ((PerHandleData->clntAddr.sin_port == gState[i].addr[j].sin_port) && (PerHandleData->clntAddr.sin_addr.S_un.S_addr == gState[i].addr[j].sin_addr.S_un.S_addr))
					{
						gState[i].usr[j].pVec = PerIoData->wsaBuf;
						gState[i].usr[j].pVec.len = BytesTransferred+2;

						switch (gState[i].count)
						{
						case 1:
							gState[i].usr[j].pVec.buf[BytesTransferred] = '/';
							gState[i].usr[j].pVec.buf[BytesTransferred + 1] = 'a';
							gState[i].usr[j].pVec.len = BytesTransferred + 2;
							WSASend(PerHandleData->hClntSock, &gState[i].usr[j].pVec, 1, NULL, 0, NULL, NULL);
							break;
						case 2:
							gState[i].usr[j].pVec.buf[BytesTransferred] = '/';
							gState[i].usr[j].pVec.buf[BytesTransferred + 1] = 'a';
							gState[i].usr[j].pVec.len = BytesTransferred + 2;
							gState[i].usr[1-j].pVec.buf[BytesTransferred] = '/';
							gState[i].usr[1-j].pVec.buf[BytesTransferred + 1] = 'b';
							gState[i].usr[1-j].pVec.len = BytesTransferred + 2;
							WSASend(PerHandleData->hClntSock, &gState[i].usr[(1 - j)].pVec, 1, NULL, 0, NULL, NULL);
							break;
						case 3:
							gState[i].usr[j].pVec.buf[BytesTransferred] = '/';
							gState[i].usr[j].pVec.buf[BytesTransferred + 1] = 'a';
							gState[i].usr[(j+1)%3].pVec.buf[BytesTransferred] = '/';
							gState[i].usr[(j+1)%3].pVec.buf[BytesTransferred + 1] = 'b';
							gState[i].usr[(j+2)%3].pVec.buf[BytesTransferred] = '/';
							gState[i].usr[(j+2)%3].pVec.buf[BytesTransferred + 1] = 'c';
							gState[i].usr[j].pVec.len = BytesTransferred + 2;
							gState[i].usr[(j+1)%3].pVec.len = BytesTransferred + 2;
							gState[i].usr[(j+2)%3].pVec.len = BytesTransferred + 2;
							WSASend(PerHandleData->hClntSock, &gState[i].usr[(j+2)%3].pVec, 1, NULL, 0, NULL, NULL);
							WSASend(PerHandleData->hClntSock, &gState[i].usr[(j+1)%3].pVec, 1, NULL, 0, NULL, NULL);
							break;
						case 4:
							gState[i].usr[j].pVec.buf[BytesTransferred] = '/';
							gState[i].usr[j].pVec.buf[BytesTransferred + 1] = 'a';
							gState[i].usr[(j + 1) % 4].pVec.buf[BytesTransferred] = '/';
							gState[i].usr[(j + 1) % 4].pVec.buf[BytesTransferred + 1] = 'b';
							gState[i].usr[(j + 2) % 4].pVec.buf[BytesTransferred] = '/';
							gState[i].usr[(j + 2) % 4].pVec.buf[BytesTransferred + 1] = 'c';
							gState[i].usr[(j+3)%4].pVec.buf[BytesTransferred] = '/';
							gState[i].usr[(j+3)%4].pVec.buf[BytesTransferred + 1] = 'd';
							gState[i].usr[j].pVec.len = BytesTransferred + 2;
							gState[i].usr[(j + 1) % 4].pVec.len = BytesTransferred + 2;
							gState[i].usr[(j + 2) % 4].pVec.len = BytesTransferred + 2;
							gState[i].usr[(j+3)%4].pVec.len = BytesTransferred + 2;
							WSASend(PerHandleData->hClntSock, &gState[i].usr[(j + 1) % 4].pVec, 1, NULL, 0, NULL, NULL);
							WSASend(PerHandleData->hClntSock, &gState[i].usr[(j + 2) % 4].pVec, 1, NULL, 0, NULL, NULL);
							WSASend(PerHandleData->hClntSock, &gState[i].usr[(j + 3) % 4].pVec, 1, NULL, 0, NULL, NULL);
							break;
						}
						break;
					}
				}
			}
		} 
		// Send
		// PerIoData->wsaBuf.len = BytesTransferred;
		// WSASend(PerHandleData->hClntSock, &(PerIoData->wsaBuf), 1, NULL, 0, NULL, NULL);

		// Receive
		memset(&(PerIoData->overlapped), 0, sizeof(OVERLAPPED));
		PerIoData->wsaBuf.len = BUFSIZE;
		PerIoData->wsaBuf.buf = PerIoData->buffer;
		flags = 0;
		WSARecv(PerHandleData->hClntSock,
			&(PerIoData->wsaBuf),
			1,
			NULL,
			&flags,
			&(PerIoData->overlapped),
			NULL
			);
	}
	return 0;
}

void ErrorHandling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}