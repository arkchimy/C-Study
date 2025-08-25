#include "stdafx.h"
#include "../lib/CrushDump_lib/CrushDump_lib/CrushDump_lib.h"
#include "../../_1Course/lib/Profiler_lib/Profiler_lib.h"

#include "CTestServer.h"
#include <conio.h>

static CTestServer EchoServer;
CDump cump;

int main()
{
    CDump::SetHandlerDump();

    st_WSAData wsa;

    wchar_t bindAddr[16];
    short bindPort;

    int iWorkerCnt;

    int iZeroCopy;
    int WorkerThreadCnt;
    int reduceThreadCount;
    int NoDelay;
    int maxSessions;
    int ZeroByteTest;
    LINGER linger;

    {
        Parser parser;

        if (parser.LoadFile(L"Config.txt") == false)
            ERROR_FILE_LOG(L"ParserError.txt", L"LoadFile");
        parser.GetValue(L"ServerAddr", bindAddr, 16);
        parser.GetValue(L"ServerPort", bindPort);

        parser.GetValue(L"LingerOn", linger.l_onoff);
        parser.GetValue(L"ZeroCopy", iZeroCopy);
        parser.GetValue(L"ZeroByteTest", ZeroByteTest);
        parser.GetValue(L"WorkerThreadCnt", WorkerThreadCnt);
        parser.GetValue(L"ReduceThreadCount", reduceThreadCount);
        parser.GetValue(L"NoDelay", NoDelay);
        parser.GetValue(L"MaxSessions", maxSessions);
        EchoServer.Start(bindAddr, bindPort, iZeroCopy, WorkerThreadCnt, reduceThreadCount, NoDelay, maxSessions, ZeroByteTest);
    }
  

    while (1)
    {
        if (_kbhit())
        {
            char ch = _getch();

            if (ch == VK_ESCAPE)
            {
                SetEvent(EchoServer.m_ServerOffEvent);

            }
            else if (ch == 'a' || ch == 'A')
                PROFILE_Manager::Instance.createProfile();
            else if (ch == 'd' || ch == 'D')
                PROFILE_Manager::Instance.resetInfo();
        }
    }

    //WaitForMultipleObjects();
    return 0;
}