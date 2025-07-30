#include <iostream>

#include <conio.h>
#include <iomanip>
#include <list>
#include <map>
#include <string>
#include <thread>
#include <windows.h>

#include "../../_1Course/lib/Parser_lib/Parser_lib.h"
#include "../lib/MT_CRingBuffer_lib/framework.h"

#pragma comment(lib, "winmm")

#define NULLCHECK_CREATE_EVENT(x, manual)             \
    {                                                 \
        x = CreateEvent(nullptr, manual, 0, nullptr); \
        if (x == nullptr)                             \
        {                                             \
            printf("CreateEvent Fail \n");            \
            __debugbreak();                           \
        }                                             \
    }
#define MSG_LOG_PRINTF(x) \
    {                     \
        std::cout << x;   \
    }

#define dfJOB_ADD 0
#define dfJOB_DEL 1
#define dfJOB_SORT 2
#define dfJOB_FIND 3
#define dfJOB_PRINT 4 //<< 출력 or 저장 / 읽기만 하는 느린 행동
#define dfJOB_QUIT 5
#define dfJOB_MAX 6

#define RINGBUFFER_SIZE 3000000
using tpsType = long long;

struct st_MSG_HEAD
{
    short shType;
    short shPayLoadLen;
};

std::map<DWORD, DWORD> m_Tps;
tpsType msg_count[dfJOB_MAX];

SRWLOCK ringBuffer_srw;
SRWLOCK list_srw;
DWORD frameInterval;

HANDLE hExitEvent, hStartEvent, hMSG_Event;
int workerCnt = 0;

CRingBuffer ringBuffer(RINGBUFFER_SIZE);
std::list<std::wstring> g_List;

unsigned WorkerThread(void *arg);
unsigned MonitorThread(void *arg);

void Initalize(HANDLE **hWorkerThread);
void CreateMsg();
void CreateExitMsg();
void MSG_count(short shType);

int main()
{
    timeBeginPeriod(1);
    HANDLE *hWorkerThread;
    HANDLE hMonitorThread;

    DWORD retVal;

    Initalize(&hWorkerThread); // workerCnt 갯수 가져옴.
    hMonitorThread = (HANDLE)_beginthreadex(nullptr, 0, MonitorThread, hWorkerThread, 0, nullptr);
    NULLCHECK_CREATE_EVENT(hExitEvent, true);
    NULLCHECK_CREATE_EVENT(hStartEvent, true);
    NULLCHECK_CREATE_EVENT(hMSG_Event, false);

    InitializeSRWLock(&ringBuffer_srw);
    InitializeSRWLock(&list_srw);

    frameInterval = 20;

    Sleep(1000);
    SetEvent(hStartEvent);

    srand(1);

    while (1)
    {

        retVal = WaitForSingleObject(hExitEvent, frameInterval);
        if (retVal == WAIT_OBJECT_0)
            break;
        CreateMsg();

        if ((float)ringBuffer.GetUseSize() / RINGBUFFER_SIZE * 100.f >= 70.f)
        {
            frameInterval += 50;
            std::cout << "frameInterval : " << frameInterval << "\n";
        }
        else if ((float)ringBuffer.GetUseSize() / RINGBUFFER_SIZE * 100.f <= 20.f)
        {
            if (frameInterval > 0)
                frameInterval -= 1;
        }
        if (_kbhit())
        {
            char ch = _getch();
            if (ch == VK_ESCAPE)
                CreateExitMsg();
            if (ch == 'a' || ch == 'A')
            {
                if (frameInterval > 0)
                    frameInterval--;
                std::cout << "frameInterval : " << frameInterval << "\n";
            }
            else if (ch == 'd' || ch == 'D')
            {
                frameInterval++;
                std::cout << "frameInterval : " << frameInterval << "\n";
            }
        }
    }
    WaitForMultipleObjects(workerCnt, hWorkerThread, true, INFINITE);

    WaitForSingleObject(hMonitorThread, INFINITE);

    std::cout << "정상 종료 \n";
    timeEndPeriod(1);
    delete[] hWorkerThread;
    return 0;
}

unsigned WorkerThread(void *arg)
{
    HANDLE hWaitThread[2];
    st_MSG_HEAD head;
    ringBufferSize useSize;

    DWORD currentThreadID;

    wchar_t str[100];
    std::wstring element;
    hWaitThread[0] = hExitEvent;
    hWaitThread[1] = hMSG_Event;

    currentThreadID = GetCurrentThreadId();

    AcquireSRWLockExclusive(&ringBuffer_srw);
    m_Tps[currentThreadID] = 0;
    ReleaseSRWLockExclusive(&ringBuffer_srw);

    WaitForSingleObject(hStartEvent, INFINITE);
    printf("WorkerThread ID :  %d  StartEvent \n", currentThreadID);

    while (1)
    {

        WaitForMultipleObjects(2, hWaitThread, false, INFINITE);
    ReEnter:
        AcquireSRWLockExclusive(&ringBuffer_srw);

        useSize = ringBuffer.GetUseSize();
        if (useSize < sizeof(head))
        {
            ReleaseSRWLockExclusive(&ringBuffer_srw);
            continue;
        }

        if (ringBuffer.Peek(reinterpret_cast<char *>(&head), sizeof(head)) != sizeof(head))
            __debugbreak();
        if (useSize < sizeof(head) + head.shPayLoadLen)
        {
            ReleaseSRWLockExclusive(&ringBuffer_srw);
            continue;
        }

        if (head.shType == dfJOB_ADD || head.shType == dfJOB_FIND)
        {
          
            if (ringBuffer.Dequeue(reinterpret_cast<char *>(&head), sizeof(head)) == false)
                __debugbreak();
           
            if (ringBuffer.Dequeue((char *)str, head.shPayLoadLen) == false)
                __debugbreak();
        }
        else
        {
            ringBuffer.Dequeue(reinterpret_cast<char *>(&head), sizeof(head));
        }

        ReleaseSRWLockExclusive(&ringBuffer_srw);

        switch (head.shType)
        {
        case dfJOB_ADD:
            AcquireSRWLockExclusive(&list_srw);
            element = str;
            g_List.push_back(element);
      
            
            ReleaseSRWLockExclusive(&list_srw);
            break;
        case dfJOB_DEL:
            AcquireSRWLockExclusive(&list_srw);
            if (g_List.empty() == false)
                g_List.pop_front();
            
            ReleaseSRWLockExclusive(&list_srw);
            break;
        case dfJOB_SORT:
            AcquireSRWLockExclusive(&list_srw);
            g_List.sort();
         
            ReleaseSRWLockExclusive(&list_srw);
            break;
        case dfJOB_FIND:
            AcquireSRWLockExclusive(&list_srw);
            element.clear();
            element.append(str);
            for (auto data : g_List)
            {
                if (lstrcmpW(data.c_str(), element.c_str()) == 0)
                {
                    // printf("Find %ls  Success \n", element.c_str());
                    break;
                }
            }
       
            ReleaseSRWLockExclusive(&list_srw);
            break;
        case dfJOB_PRINT:
            AcquireSRWLockExclusive(&list_srw);
            for (int i = 0; i < 10000; i++)
            {
                YieldProcessor();
            }
            ReleaseSRWLockExclusive(&list_srw);
            Sleep(5);
            break;
        case dfJOB_QUIT:
            printf("WorkerThread ID :  %d  return \n", currentThreadID);
        
            return 0;
        }
        MSG_count(head.shType); // 메세지 카운팅
        m_Tps[currentThreadID]++;
        useSize = ringBuffer.GetUseSize();
        if (useSize >= sizeof(head))
            goto ReEnter;
    }
    printf("WorkerThread ID :  %d  return \n", currentThreadID);
    return 0;
}

unsigned MonitorThread(void *arg)
{
    HANDLE *hThread = (HANDLE *)arg;

    DWORD retval, currentThreadID;
    ringBufferSize MonitorUsesize, MonitorFreeSize;

    currentThreadID = GetCurrentThreadId();

    Sleep(2000);
    while (1)
    {
        retval = WaitForMultipleObjects(workerCnt, hThread, true, 1000);
        if (WAIT_TIMEOUT != retval)
            break;
        MonitorUsesize = ringBuffer.GetUseSize();

        // PrintfMsg_TYPE();
        // PrintfTPS();
        // PrintfMsgQ();
        std::cout << "====================== frameInterval :  " << frameInterval << " g_List  :  " << g_List.size() << "========================= \n ";

        for (auto &element : m_Tps)
        {
            std::cout << "Thread ID :" << element.first << " TPS [ " << element.second << " ]\n";
            element.second = 0;
        }
        std::cout << "=================   msg Packet ==================== \n";
        for (int i = 0; i < dfJOB_MAX; i++)
        {
            switch (i)
            {
            case dfJOB_ADD:
                std::cout << "dfJOB_ADD :";
                break;
            case dfJOB_DEL:
                std::cout << "dfJOB_DEL :";
                break;
            case dfJOB_SORT:
                std::cout << "dfJOB_SORT :";
                break;
            case dfJOB_FIND:
                std::cout << "dfJOB_FIND :";
                break;
            case dfJOB_PRINT:
                std::cout << "dfJOB_PRINT :";
                break;
            case dfJOB_QUIT:
                std::cout << "dfJOB_QUIT :";
                break;
            }
            std::cout << " TPS [ " << std::left << msg_count[i] << " ] \n";
            msg_count[i] = 0;
        }
        std::cout << " \n";
        std::cout << "RingBuffer  :  " << (float)MonitorUsesize / RINGBUFFER_SIZE * 100.f << "%  \n";
        std::cout << "=========================================================== \n";
    }
    printf("MonitorThread ID :  %d  return \n", currentThreadID);
    return 0;
}

void Initalize(HANDLE **pphWorkerThread)
{
    Parser parser;
    if (parser.LoadFile(L"Config.txt") == false)
        printf("File_OpenFail\n");
    parser.GetValue(L"WorkerThreadCnt", workerCnt);

    *pphWorkerThread = new HANDLE[workerCnt];

    for (int i = 0; i < workerCnt; i++)
    {

        (*pphWorkerThread)[i] = (HANDLE)_beginthreadex(nullptr, 0, WorkerThread, nullptr, 0, nullptr);
        if ((*pphWorkerThread)[i] == nullptr)
        {
            printf(" Thread Cnt %d  CreateFail\n", i);
            __debugbreak();
        }
    }
}

void CreateMsg()
{
    static const std::wstring chars = L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    static const int strlen = lstrlenW(chars.c_str());

    wchar_t EnqStr[100];
    st_MSG_HEAD head;
    size_t list_len;

    AcquireSRWLockShared(&list_srw);
    list_len = g_List.size();
    ReleaseSRWLockShared(&list_srw);
    head.shType = rand() % dfJOB_QUIT;

    switch (head.shType)
    {
    case dfJOB_ADD:
        head.shPayLoadLen = (rand() % strlen) * sizeof(wchar_t);
        memcpy(EnqStr, chars.c_str(), head.shPayLoadLen);
       
        break;

    case dfJOB_DEL:
        head.shPayLoadLen = 0;
        break;

    case dfJOB_SORT:
        head.shPayLoadLen = 0;
        break;
    case dfJOB_FIND:
        head.shPayLoadLen = (rand() % strlen) * sizeof(wchar_t);
        memcpy(EnqStr, chars.c_str(), head.shPayLoadLen);
        EnqStr[head.shPayLoadLen / 2] = 0;
        break;

    case dfJOB_PRINT:
        head.shPayLoadLen = 0;
        break;
    }

    while (sizeof(st_MSG_HEAD) + head.shPayLoadLen > ringBuffer.GetFreeSize())
    {
        YieldProcessor();
    }

    ringBuffer.Enqueue(reinterpret_cast<char *>(&head), sizeof(st_MSG_HEAD));
    ringBuffer.Enqueue(reinterpret_cast<char *>(&EnqStr), head.shPayLoadLen);

    SetEvent(hMSG_Event);
}

void CreateExitMsg()
{
    st_MSG_HEAD head;
    head.shType = dfJOB_QUIT;
    head.shPayLoadLen = 0;

    for (int i = 0; i < workerCnt; i++)
    {
        while (sizeof(st_MSG_HEAD) + head.shPayLoadLen > ringBuffer.GetFreeSize())
        {
            YieldProcessor();
        }
        ringBuffer.Enqueue(reinterpret_cast<char *>(&head), sizeof(st_MSG_HEAD));
    }
    SetEvent(hExitEvent);
}

void MSG_count(short shType)
{
    _InterlockedIncrement64(&msg_count[shType]);
}
