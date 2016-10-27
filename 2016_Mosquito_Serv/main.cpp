#pragma comment(lib, "ws2_32.lib")

#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <process.h>

#define BUFSIZE 1024

typedef struct //�������� ����ü
{
	SOCKET hClntSock;
	SOCKADDR_IN clntAddr;
} PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

typedef struct // ���Ϲ������� ����ü
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
	GetSystemInfo(&SystemInfo);	// �ý��� ������ ����. CPU ������ŭ ������ ����� ���� �ʿ�(dwNumberOfProcessors)

	// �����带 CPU ������ŭ ����
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
		printf("[����] Ŭ���̾�Ʈ ���� : IP �ּ� = %s, ��Ʈ = %d\n",
			inet_ntoa(clntAddr.sin_addr), ntohs(clntAddr.sin_port));
		
		for (int u = 0; u < 750; u++)
		{
			if (gState[u].flag == false)
			{
				gState[u].flag = true;
				gState[u].usr[gState[u].count].cnFlag = true;
				gState[u].addr[gState[u].count] = clntAddr;
				gState[u].count++;
				printf("%d������ %d ��° �÷��̾�� �����մϴ�.\n", (u+1), gState[u].count);
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
					printf("%d������ %d ��° �÷��̾�� �����մϴ�.\n", (u+1), gState[u].count);
					break;
				}
			}
		}

		// ����� Ŭ���� ���ϰ� �ּ� ���� ����, PerHandleData = Ŭ������
		PerHandleData = (LPPER_HANDLE_DATA)malloc(sizeof(PER_HANDLE_DATA));
		PerHandleData->hClntSock = hClntSock;
		memcpy(&(PerHandleData->clntAddr), &clntAddr, addrLen);

		// CP ����(CompletionPort)
		CreateIoCompletionPort((HANDLE)hClntSock, hCompletionPort, (DWORD)PerHandleData, 0);

		// �ٽ� ����ü �ʱ�ȭ
		PerIoData = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
		memset(&(PerIoData->overlapped), 0, sizeof(OVERLAPPED));
		PerIoData->wsaBuf.len = BUFSIZE;
		PerIoData->wsaBuf.buf = PerIoData->buffer;

		Flags = 0;

		WSARecv(PerHandleData->hClntSock, // ������ �Է¼���.
			&(PerIoData->wsaBuf),  // ������ �Է� ����������.
			1,       // ������ �Է� ������ ��.
			(LPDWORD)&RecvBytes,
			(LPDWORD)&Flags,
			&(PerIoData->overlapped), // OVERLAPPED ����ü ������.
			NULL
			);
	}
	return 0;
}

//����� �Ϸῡ ���� �������� �ൿ ����
unsigned int __stdcall CompletionThread(LPVOID pComPort)
{
	HANDLE hCompletionPort = (HANDLE)pComPort;
	DWORD BytesTransferred;
	LPPER_HANDLE_DATA PerHandleData;
	LPPER_IO_DATA PerIoData;
	DWORD flags;

	while (1)
	{
		// IO ����
		GetQueuedCompletionStatus(hCompletionPort,    // Completion Port
			&BytesTransferred,   // ���۵� ����Ʈ��
			(LPDWORD)&PerHandleData,
			(LPOVERLAPPED*)&PerIoData, // OVERLAPPED ����ü ������.
			INFINITE
			);

		// ���� �����
		if (BytesTransferred == 0) //EOF ���۽�.
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