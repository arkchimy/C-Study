// EnCordingSendClient.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <WS2tcpip.h>
#include <iostream>
#pragma comment(lib, "ws2_32")
#include "../../_3Course/lib/SerializeBuffer_exception/SerializeBuffer_exception.h"
#include "../../_3Course/lib/CrushDump_lib/CrushDump_lib/CrushDump_lib.h"

CDump dump;

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
    linger linger;
    stHeader header;
    {
 
        int buflen;

        DWORD bind_retval;

        linger.l_onoff = 1;
        linger.l_linger = 0;

        ZeroMemory(&serverAddr, sizeof(serverAddr));

        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        InetPtonW(AF_INET, str.c_str(), &serverAddr.sin_addr);

    }
    while (1)
    {
    retry:

        m_Socket = socket(AF_INET, SOCK_STREAM, 0);
        if (m_Socket == INVALID_SOCKET)
        {
            Sleep(1000);
            goto retry;
        }
        setsockopt(m_Socket, SOL_SOCKET, SO_LINGER, (const char *)&linger, sizeof(linger));

        Connect_retval = connect(m_Socket, (sockaddr *)&serverAddr, sizeof(serverAddr));
        if (Connect_retval != 0)
        {
            static int cnt = 1;
            Sleep(1000);
            std::cout << cnt++ << "번쨰 시도 중\n";
            goto retry;
        }
        else
        {
            std::cout << " Connected\n";
        }
        char temp[1000];
        recv(m_Socket, (char *)temp, 100, 0);
        int recvRetval;
        int sendRetval;
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

            sendRetval = send(m_Socket, (char *)clsSendMessage._begin, header.sDataLen + sizeof(header), 0);
            if (sendRetval == -1)
            {
                std::cout << "연결이 끊어졌습니다.\n";
                closesocket(m_Socket);
                goto retry;
            }
            recvRetval = recv(m_Socket, (char *)clsRecvMessage._begin, header.sDataLen + sizeof(header), 0);

            if (recvRetval == -1)
            {
                std::cout << "연결이 끊어졌습니다.\n";
                closesocket(m_Socket);
                goto retry;
            }
            clsRecvMessage._rearPtr = clsRecvMessage._begin + recvRetval;
            if (clsRecvMessage.DeCoding() == false)
            {
                // 보낸 데이터와 받은 데이터가 다르다면  __debugbreak;
                clsSendMessage.HexLog(CMessage::en_Tag::ENCODE);// 보낸 인코딩 된 데이터
                clsRecvMessage.HexLog(CMessage::en_Tag::DECODE);// 받은 데이터를 디코딩한 것
                __debugbreak();
            }
        }
    }
}
