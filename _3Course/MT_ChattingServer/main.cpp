#include "CMTChattingServer.h"
#include <conio.h>
#include "../lib/CrushDump_lib/CrushDump_lib/CrushDump_lib.h"
#include "../lib/Profiler_MultiThread/Profiler_MultiThread.h"

#include <thread>

int main()
{
    CDump::SetHandlerDump();

    st_WSAData wsa;

    wchar_t bindAddr[16];
    short bindPort;

    int iZeroCopy;
    int iEnCording;
    int WorkerThreadCnt ,ContentsThreadCnt;
    int reduceThreadCount;
    int NoDelay;
    int maxSessions;

    LINGER linger;
    int iRingBufferSize;
    int ContentsRingBufferSize;

    HRESULT hr;

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


        CRingBuffer::s_BufferSize = iRingBufferSize;
    }
    wchar_t buffer[100];

    StringCchPrintfW(buffer, 100, L"Profiler_%hs.txt", __DATE__);

    {
        CTestServer::s_ContentsQsize = ContentsRingBufferSize;
        CTestServer *ChattingServer = new CTestServer(ContentsThreadCnt, iEnCording);

        ChattingServer->Start(bindAddr, bindPort, iZeroCopy, WorkerThreadCnt, reduceThreadCount, NoDelay, maxSessions);

        CSystemLog::GetInstance()->SetDirectory(L"SystemLog");
        CSystemLog::GetInstance()->SetLogLevel(en_LOG_LEVEL::ERROR_Mode);
        while (1)
        {
            if (GetAsyncKeyState(VK_ESCAPE))
            {
                SetEvent(ChattingServer->m_ServerOffEvent);
                break;
            }
            if (_kbhit())
            {
                char ch = _getch();
                if (ch == 'A' || ch == 'a')
                {
                    CSystemLog::GetInstance()->SaveAsLog();
                    Profiler::SaveAsLog(buffer);
                }
                if (ch == 'D' || ch == 'd')
                {
                    Profiler::Reset();
                }
                if (ch == '1')
                {

                    CSystemLog::GetInstance()->SetLogLevel(en_LOG_LEVEL::ERROR_Mode);
                    printf("ERROR_Mode\n");
                }
                if (ch == '2')
                {

                    CSystemLog::GetInstance()->SetLogLevel(en_LOG_LEVEL::DEBUG_TargetMode);
                    printf("DEBUG_TargetMode\n");
                }
                if (ch == '3')
                {

                    CSystemLog::GetInstance()->SetLogLevel(en_LOG_LEVEL::DEBUG_Mode);
                    printf("DEBUG_Mode\n");
                }
            }
        }

        Sleep(1000);
    }
   
}
