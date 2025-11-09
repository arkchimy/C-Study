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

#include "../../_3Course/lib/CNetwork_lib/stHeader.h"

int main()
{
    st_WSAData wsa;
    short port;
    std::wstring str;

    std::cout << "IP Address : ";
    std::wcin >> str;

    std::cout << "Port : ";
    std::cin  >> port;
    

    SOCKET m_Socket;
    SOCKADDR_IN serverAddr;
    int Connect_retval;

    char buffer[100];
    char DataStr[] = "aaaaaaaaaabbbbbbbbbbcccccccccc1234567890abcdefghijklmn";

    stHeader header;
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
            
            CMessage clsSendMessage;
            CMessage clsRecvMessage;
            

            //struct stHeader
            //{
            //  public:
            //    BYTE byCode;
            //    BYTE byType; //
            //    SHORT sDataLen;
            //    BYTE byRandKey;
            //    BYTE byCheckSum;
            //};
            header.byCode = 0x89;
            header.byType = 250;
            header.sDataLen = sizeof(DataStr);
 
            clsSendMessage.PutData((char *)&header, sizeof(header));
            clsSendMessage.PutData((char *)DataStr, sizeof(DataStr));

            clsSendMessage.EnCoding();

            send(m_Socket, (char *)clsSendMessage._begin, header.sDataLen + sizeof(header), 0);
            recvRetval = recv(m_Socket, (char *)clsRecvMessage._begin, header.sDataLen + sizeof(header), 0);
            clsRecvMessage._rearPtr = clsRecvMessage._begin + recvRetval;
            if (clsRecvMessage.DeCoding() == false)
            {
                clsSendMessage.HexLog(CMessage::en_Tag::NORMAL);// 보낸 데이터
                clsRecvMessage.HexLog(CMessage::en_Tag::DECODE);// 받은 데이터를 디코딩한 것
                __debugbreak();
            }
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
