
#include "stdafx.h"
#include "../lib/CrushDump_lib/CrushDump_lib/CrushDump_lib.h"
#include "../lib/Profiler_MultiThread/Profiler_MultiThread.h"
#include "CTestServer.h"
#include <conio.h>

CDump cump;

int main()
{
    CDump::SetHandlerDump();

    st_WSAData wsa;

    wchar_t bindAddr[16];
    short bindPort;

    int iZeroCopy;
    int WorkerThreadCnt;
    int reduceThreadCount;
    int NoDelay;
    int maxSessions;
    int ZeroByteTest;
    LINGER linger;
    int iRingBufferSize;
    int ContentsRingBufferSize;

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

        parser.GetValue(L"RingBufferSize", iRingBufferSize);
        parser.GetValue(L"ContentsRingBufferSize", ContentsRingBufferSize);
        CRingBuffer::s_BufferSize = iRingBufferSize;
    }
    wchar_t buffer[100];

     StringCchPrintfW(buffer, 100, L"Profiler_%hs.txt", __DATE__);
    {
        CTestServer::s_ContentsQsize = ContentsRingBufferSize;
        CTestServer EchoServer;
        EchoServer.Start(bindAddr, bindPort, iZeroCopy, WorkerThreadCnt, reduceThreadCount, NoDelay, maxSessions, ZeroByteTest);

        while (1)
        {
            if (GetAsyncKeyState(VK_ESCAPE))
            {
                SetEvent(EchoServer.m_ServerOffEvent);
            }
            else if (GetAsyncKeyState('A') || GetAsyncKeyState('A'))
                Profiler::SaveAsLog(buffer);
            else if (GetAsyncKeyState('D') || GetAsyncKeyState('d'))
            {
                Profiler::Reset();
            }
        
        }
    }

    // WaitForMultipleObjects();
    return 0;
}