#include <iostream>

#include <WS2tcpip.h>
#include <WinSock2.h>

#include    "../../_1Course/lib/Parser_lib/Parser_lib.h"
#include    "../../error_log.h"

#pragma comment(lib, "winmm")
#pragma comment(lib, "ws2_32")

int main()
{
    int clientCnt;

    LINGER linger;
    int disconnect;
    int bZeroCopy;
    int WorkerThreadCnt;

    linger.l_linger = 0;

    {
        Parser parser;
        if (parser.LoadFile(L"EchoClient_Config.txt") == false)
            ERROR_FILE_LOG(L"ParserError.txt", L"LoadFile");
        parser.GetValue(L"ClientCnt", clientCnt);
        parser.GetValue(L"Disconnect", disconnect);
        parser.GetValue(L"LingerOn", linger.l_onoff);
        parser.GetValue(L"ZeroCopy", bZeroCopy);
        parser.GetValue(L"WorkerThreadCnt", WorkerThreadCnt);
    }
    
    
}

