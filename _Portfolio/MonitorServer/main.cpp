#include "CTestServer.h"
#include "../../_3Course/lib/CrushDump_lib/CrushDump_lib/CrushDump_lib.h"

CDump dump;

int main()
{

    CDump::SetHandlerDump();

    stWSAData wsa;
    CTestServer server(true);

    Parser parser;
    if (parser.LoadFile(L"Config.txt") == false)
        printf("LoadFile Fail\n");
    else

    {
        wchar_t bindAddr[16];
        unsigned short port;
        int ZeroCopy;
        int WorkerCreateCnt;
        int reduceThreadCount;
        int noDelay;
        int MaxSessions;
        
        //Start(const wchar_t *bindAddress, short port, int ZeroCopy, int WorkerCreateCnt, int reduceThreadCount, int noDelay, int MaxSessions)

        parser.GetValue(L"MonitorServer_IP_Address", bindAddr, 16);
        parser.GetValue(L"MonitorServer_IP_Port", port);

        parser.GetValue(L"ZeroCopy", ZeroCopy);
        parser.GetValue(L"WorkerCreateCnt", WorkerCreateCnt);
        parser.GetValue(L"reduceThreadCount", reduceThreadCount);
        parser.GetValue(L"noDelay", noDelay);
        parser.GetValue(L"MaxSessions", MaxSessions);

        server.Start(bindAddr, port,  ZeroCopy, WorkerCreateCnt, reduceThreadCount, noDelay, MaxSessions);

    }
    
    while (1)
    {

    }

    return 0;
}