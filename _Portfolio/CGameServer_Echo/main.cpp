// CGameServer_Echo.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <iostream>
#include "CTestServer.h"
#include "../../_3Course/lib/CrushDump_lib/CrushDump_lib/CrushDump_lib.h"


int main()
{
    CDump::SetHandlerDump();
    stWSAData wsa;

    wchar_t bindAddr[16];
    wchar_t RedisIpAddress[16];
    short bindPort;

    int iZeroCopy;
    int iEnCording;
    int WorkerThreadCnt, ContentsThreadCnt;
    int reduceThreadCount;
    int NoDelay;
    int maxSessions;

    LINGER linger{1, 0};
    int iRingBufferSize;
    int ContentsRingBufferSize;

    CTestServer *server = new CTestServer(true);
    {
        Parser parser;

        if (parser.LoadFile(L"Config.txt") == false)
            CSystemLog::GetInstance()->Log(L"ParserError.txt", en_LOG_LEVEL::ERROR_Mode, L"LoadFileError %d", GetLastError());
        parser.GetValue(L"ServerAddr", bindAddr, 16);
        parser.GetValue(L"ServerPort", bindPort);

        parser.GetValue(L"LingerOn", linger.l_onoff);
        parser.GetValue(L"ZeroCopy", iZeroCopy);

        parser.GetValue(L"WorkerThreadCnt", WorkerThreadCnt);
        parser.GetValue(L"ReduceThreadCount", reduceThreadCount);
        parser.GetValue(L"NoDelay", NoDelay);
        parser.GetValue(L"MaxSessions", maxSessions);

        parser.GetValue(L"ContentsThreadCnt", ContentsThreadCnt);
        parser.GetValue(L"RingBufferSize", iRingBufferSize);
        parser.GetValue(L"ContentsRingBufferSize", ContentsRingBufferSize);
        parser.GetValue(L"EnCording", iEnCording);

        parser.GetValue(L"RedisIpAddress", RedisIpAddress, CZoneServer::IP_LEN);

        CRingBuffer::s_BufferSize = iRingBufferSize;
    }
    size_t i;
    wcstombs_s(&i, server->RedisIpAddress, CZoneServer::IP_LEN, RedisIpAddress, CZoneServer::IP_LEN);

    server->Start(bindAddr, bindPort, iZeroCopy, WorkerThreadCnt, reduceThreadCount, NoDelay, maxSessions);

    while (1)
    {

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
