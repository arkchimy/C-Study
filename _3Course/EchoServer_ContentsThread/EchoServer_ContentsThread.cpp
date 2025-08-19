#include "stdafx.h"
#include "CTestServer.h"

static CTestServer EchoServer;

int main()
    {
    st_WSAData wsa;



    wchar_t bindAddr[16];
    short bindPort;

    int iWorkerCnt;

    int bZeroCopy;
    int WorkerThreadCnt;
    int reduceThreadCount;
    int NoDelay;
    int maxSessions;
    LINGER linger;

    {
        Parser parser;

        if (parser.LoadFile(L"Config.txt") == false)
            ERROR_FILE_LOG(L"ParserError.txt", L"LoadFile");
        parser.GetValue(L"ServerAddr", bindAddr, 16);
        parser.GetValue(L"ServerPort", bindPort);
        parser.GetValue(L"iWorkerCnt", iWorkerCnt);

        parser.GetValue(L"LingerOn", linger.l_onoff);
        parser.GetValue(L"ZeroCopy", bZeroCopy);
        parser.GetValue(L"WorkerThreadCnt", WorkerThreadCnt);
        parser.GetValue(L"ReduceThreadCount", reduceThreadCount);
        parser.GetValue(L"NoDelay", NoDelay);
        parser.GetValue(L"MaxSessions", maxSessions);
        EchoServer.Start(bindAddr, bindPort, iWorkerCnt, reduceThreadCount, NoDelay, maxSessions);
    }

    while (1)
    {

    }

    return 0;
}