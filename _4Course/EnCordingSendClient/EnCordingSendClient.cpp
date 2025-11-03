// EnCordingSendClient.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <WS2tcpip.h>
#include <iostream>
#pragma comment(lib, "ws2_32")
#include "../../_3Course/lib/SerializeBuffer_exception/SerializeBuffer_exception.h"

class st_WSAData
{
  public:
    st_WSAData()
    {
        WSAData wsa;
        DWORD wsaStartRetval;

        wsaStartRetval = WSAStartup(MAKEWORD(2, 2), &wsa);
        if (wsaStartRetval != 0)
        {
            __debugbreak();
        }
    }
    ~st_WSAData()
    {
        WSACleanup();
    }
};

#include <sstream>
#include <string>

#pragma pack(1)
struct stEnCordingHeader
{
    // Code(1byte) - Len(2byte) - RandKey(1byte) - CheckSum(1byte)
    BYTE Code;
    SHORT _len;
    BYTE RandKey;
    BYTE CheckSum;
};
#pragma pack()

void EnCording(CMessage *msg, BYTE K, BYTE RK)
{
    SerializeBufferSize len = msg->_rearPtr - msg->_frontPtr;
    BYTE total = 0;

    int current = 0;
    BYTE P = 0;
    BYTE E = 0;

    for (int i = 1; i <= len; i++)
    {
        total += msg->_frontPtr[i];
    }
    memcpy(msg->_frontPtr, &total, sizeof(total));

    msg->HexLog();

    for (; &msg->_frontPtr[current] != msg->_rearPtr; current++)
    {
        BYTE D1 = (msg->_frontPtr)[current];
        BYTE b = (P + RK + current);

        P = D1 ^ b;
        E = P ^ (E + K + current);
        msg->_frontPtr[current] = E;
    }
    msg->HexLog();
}

void DeCording(CMessage *msg, BYTE K, BYTE RK)
{
    BYTE P1 = 0, P2;
    BYTE E1 = 0, E2;
    BYTE D1 = 0, D2;
    BYTE total = 0;
    // 디코딩의 msg는 링버퍼에서 꺼낸 데이터로 내가 작성하는
    SerializeBufferSize len = msg->_rearPtr - msg->_frontPtr;
    int current = 0;

    msg->HexLog();
    // 2기준
    // D2 ^ (P1 + RK + 2) = P2
    // P2 ^ (E1 + K + 2) = E2

    // E2 ^ (E1 + K + 2) = P2
    // P2 ^ (P1 + RK + 2) = D2
    for (; &msg->_frontPtr[current] != msg->_rearPtr; current++)
    {
        E2 = msg->_frontPtr[current];
        P2 = E2 ^ (E1 + K + current);
        E1 = E2;
        D2 = P2 ^ (P1 + RK + current);
        P1 = P2;
        msg->_frontPtr[current] = D2;
    }

    for (int i = 1; i < len; i++)
    {
        total += msg->_frontPtr[i];
    }
    memcpy(msg->_frontPtr, &total, sizeof(total));

    msg->HexLog();
}

int main()
{
    st_WSAData wsa;
    short port;
    std::wstring str;

    std::cin >> port;
    std::wcin >> str;

    SOCKET m_Socket;
    SOCKADDR_IN serverAddr;
    int Connect_retval;

    char buffer[100];
    char DataStr[] = "aaaaaaaaaabbbbbbbbbbcccccccccc1234567890abcdefghijklmn";
    stEnCordingHeader header;
    {
        linger linger;
        int buflen;

        DWORD bind_retval;

        linger.l_onoff = 1;
        linger.l_linger = 0;

        ZeroMemory(&serverAddr, sizeof(serverAddr));

        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        InetPtonW(AF_INET, str.c_str(), &serverAddr.sin_addr);

        m_Socket = socket(AF_INET, SOCK_STREAM, 0);
        if (m_Socket == INVALID_SOCKET)
        {
            __debugbreak();
        }
        setsockopt(m_Socket, SOL_SOCKET, SO_LINGER, (const char *)&linger, sizeof(linger));
    }
    while (1)
    {
        Connect_retval = connect(m_Socket, (sockaddr *)&serverAddr, sizeof(serverAddr));
        if (Connect_retval != 0)
            __debugbreak();
        else
        {
            std::cout << " Connected\n";
        }
        char temp[1000];
        recv(m_Socket, (char *)temp, 100, 0);
        int recvRetval;
        while (1)
        {
            
            CMessage msg;

            header.Code = 0xa9;
            header.RandKey = 0x31;
            header._len = sizeof(DataStr);

            msg._frontPtr = msg._begin + offsetof(stEnCordingHeader, CheckSum);

            msg.PutData((char *)&header, sizeof(header));
            msg.PutData((char *)DataStr, sizeof(DataStr));

            EnCording(&msg, header.Code, header.RandKey);

            send(m_Socket, (char *)msg._begin, header._len + sizeof(header), 0);
            recvRetval = recv(m_Socket, (char *)msg._begin, header._len + sizeof(header), 0);

            msg._frontPtr = msg._begin + offsetof(stEnCordingHeader, CheckSum);
            msg._rearPtr = msg._begin + recvRetval;

            DeCording(&msg, header.Code, header.RandKey);
            
            if (recvRetval == header._len + sizeof(header))
            {
                for (int i = 0; i < sizeof(DataStr); i++)
                {
                    if (msg._frontPtr[i+1] != (BYTE)DataStr[i])
                        __debugbreak();
                }
            }
            else
                __debugbreak();
        }
    }
}

// 프로그램 실행: <Ctrl+F5> 또는 [디버그] > [디버깅하지 않고 시작] 메뉴
// 프로그램 디버그: <F5> 키 또는 [디버그] > [디버깅 시작] 메뉴

// 시작을 위한 팁:
//   1. [솔루션 탐색기] 창을 사용하여 파일을 추가/관리합니다.
//   2. [팀 탐색기] 창을 사용하여 소스 제어에 연결합니다.
//   3. [출력] 창을 사용하여 빌드 출력 및 기타 메시지를 확인합니다.
//   4. [오류 목록] 창을 사용하여 오류를 봅니다.
//   5. [프로젝트] > [새 항목 추가]로 이동하여 새 코드 파일을 만들거나, [프로젝트] > [기존 항목 추가]로 이동하여 기존 코드 파일을 프로젝트에 추가합니다.
//   6. 나중에 이 프로젝트를 다시 열려면 [파일] > [열기] > [프로젝트]로 이동하고 .sln 파일을 선택합니다.
