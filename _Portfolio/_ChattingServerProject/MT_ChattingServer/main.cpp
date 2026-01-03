#include <conio.h>
#include <thread>

#include "CMTChattingServer.h"
#include "CrushDump_lib/CrushDump_lib.h"

int main()
{
    CDump::SetHandlerDump();
    stWSAData wsa;

    wchar_t bindAddr[16];
    wchar_t RedisIpAddress[16];
    short bindPort;

    int iZeroCopy;
    int iEnCording;
    int WorkerThreadCnt ,ContentsThreadCnt;
    int reduceThreadCount;
    int NoDelay;
    int maxSessions;

    LINGER linger{1,0};
    int iRingBufferSize;
    int ContentsRingBufferSize;

    DWORD waitThread_Retval; // Waitfor 종료절차  반환값. 현재는 infinite

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

        parser.GetValue(L"RedisIpAddress", RedisIpAddress, IP_LEN);

        CRingBuffer::s_BufferSize = iRingBufferSize;
    }
    wchar_t buffer[100];
    CSystemLog::GetInstance()->SetDirectory(L"SystemLog");


    StringCchPrintfW(buffer, 100, L"Profiler_%hs.txt", __DATE__);

    {
        CTestServer *ChattingServer = new CTestServer(ContentsThreadCnt, iEnCording);

        size_t i;
        wcstombs_s(&i, ChattingServer->RedisIpAddress, IP_LEN, RedisIpAddress, IP_LEN);


        
        ChattingServer->Start(bindAddr, bindPort, iZeroCopy, WorkerThreadCnt, reduceThreadCount, NoDelay, maxSessions);
        CSystemLog::GetInstance()->SetLogLevel(en_LOG_LEVEL::ERROR_Mode);
        while (1)
        {
            if (GetAsyncKeyState(VK_ESCAPE))
            {
                CSystemLog::GetInstance()->Log(L"SystemLog.txt", en_LOG_LEVEL::SYSTEM_Mode, L"Server Stop");
                CSystemLog::GetInstance()->Log(L"Socket_Error.txt", en_LOG_LEVEL::SYSTEM_Mode, L"Server Stop");

                ChattingServer->bMonitorThreadOn = false;
                SetEvent(ChattingServer->m_ServerOffEvent);

                ChattingServer->Stop();

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
                else if (ch == 'D' || ch == 'd')
                {
                    Profiler::Reset();
                }
                else if (ch == 'L' || ch == ';')
                {
                    clsDeadLockManager::GetInstance()->CreateLogFile_TlsInfo();
                }
                else if (ch == '1')
                {

                    CSystemLog::GetInstance()->SetLogLevel(en_LOG_LEVEL::ERROR_Mode);
                    printf("ERROR_Mode\n");
                }
                else if (ch == '2')
                {

                    CSystemLog::GetInstance()->SetLogLevel(en_LOG_LEVEL::DEBUG_TargetMode);
                    printf("DEBUG_TargetMode\n");
                }
                else if (ch == '3')
                {

                    CSystemLog::GetInstance()->SetLogLevel(en_LOG_LEVEL::DEBUG_Mode);
                    printf("DEBUG_Mode\n");
                }
                else if (ch == 'P' || ch == 'p')
                {
                    Profiler::bOn = Profiler::bOn == true ? false : true; 
                }
            }
        }
        
        waitThread_Retval = WaitForSingleObject(ChattingServer->hMonitorThread, INFINITE);
        if (waitThread_Retval == WAIT_TIMEOUT)
        {
            //TODO : 시간 정한다면 어찌할지 정하기.
            __debugbreak();
        }
        CSystemLog::GetInstance()->Log(L"SystemLog.txt", en_LOG_LEVEL::SYSTEM_Mode, L"ContentsThread_Terminate");

    }
   
}
