#include "stdafx.h"
#include "CTestServer.h"
#include "../lib/CNetwork_lib/CNetwork_lib.h"
#include "../lib/Profiler_MultiThread/Profiler_MultiThread.h"


#include <thread>

extern SRWLOCK srw_Log;
extern template PVOID stTlsObjectPool<CMessage>::Alloc();       // �Ͻ��� �ν��Ͻ�ȭ ����
extern template void stTlsObjectPool<CMessage>::Release(PVOID); // �Ͻ��� �ν��Ͻ�ȭ ����

unsigned ContentsThread(void *arg)
{
    size_t addr;
    CMessage *message, *peekMessage;
    CTestServer *server = reinterpret_cast<CTestServer *>(arg);
    char *f, *r;
    HANDLE hWaitHandle[2] = {server->m_ContentsEvent, server->m_ServerOffEvent};
    DWORD hSignalIdx;
    ringBufferSize useSize, DeQSisze;
    ull l_sessionID;

    // bool �� ���ƺ���.

    CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                   L"%-20s ",
                                   L"This is ContentsThread");
    CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::SYSTEM_Mode,
                                   L"%-20s ",
                                   L"This is ContentsThread");
    while (1)
    {
        hSignalIdx = WaitForMultipleObjects(2, hWaitHandle, false, 20);
        if (hSignalIdx - WAIT_OBJECT_0 == 1)
            break;

        f = server->m_ContentsQ._frontPtr;
        r = server->m_ContentsQ._rearPtr;
        useSize = server->m_ContentsQ.GetUseSize(f, r);

        // ID + msg  ũ�� �޼��� �ϳ��� 16Byte
        while (useSize >= 16)
        {

            DeQSisze = server->m_ContentsQ.Dequeue(&l_sessionID, sizeof(ull));
            if (DeQSisze != sizeof(ull))
                __debugbreak();

            DeQSisze = server->m_ContentsQ.Peek(&addr, sizeof(size_t));
            peekMessage = (CMessage *)addr;
            peekMessage->_begin = peekMessage->_begin;

            DeQSisze = server->m_ContentsQ.Dequeue(&addr, sizeof(size_t));
            if (DeQSisze != sizeof(size_t))
                __debugbreak();
            message = (CMessage *)addr;

            if (rand() % 1000 == 0)
            {
                CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                               L"%-10s %10s %012llu ",
                                               L"DisConnect_Event",
                                               L"seqID :", l_sessionID);
                if(server->DisconnectForContents(l_sessionID) == true)
                    stTlsObjectPool<CMessage>::Release(message);
                else
                    server->EchoProcedure(l_sessionID, message);
            }
            // TODO : ��� Type�� �ִ´ٸ� Switch���� Ž.
            else
            {
                server->EchoProcedure(l_sessionID, message);
            }
            
            f = server->m_ContentsQ._frontPtr;
            useSize -= 16;
        }
    }

    return 0;
}

unsigned MonitorThread(void *arg)
{
    CTestServer *server = reinterpret_cast<CTestServer *>(arg);

    HANDLE hWaitHandle = {server->m_ServerOffEvent};

    DWORD wait_retval;

    LONG64 *arrTPS;
    LONG64 *before_arrTPS;

    arrTPS = new LONG64[server->m_WorkThreadCnt + 1];
    before_arrTPS = new LONG64[server->m_WorkThreadCnt + 1];
    ZeroMemory(arrTPS, sizeof(LONG64) * (server->m_WorkThreadCnt + 1));
    ZeroMemory(before_arrTPS, sizeof(LONG64) * (server->m_WorkThreadCnt + 1));

    while (1)
    {
        wait_retval = WaitForSingleObject(hWaitHandle, 1000);

        if (wait_retval != WAIT_OBJECT_0)
        {
            for (int i = 0; i <= server->m_WorkThreadCnt; i++)
            {
                arrTPS[i] = server->arrTPS[i] - before_arrTPS[i];
                before_arrTPS[i] = server->arrTPS[i];
            }

            if (server->GetSessionCount() == 0)
                continue;
            printf(" ==================================\nTotal Sessions: %d\n", server->GetSessionCount());
            printf(" Total iDisCounnectCount: %llu\n", server->iDisCounnectCount);
            printf(" IdxStack Size: %lld\n", server->Get_IdxStack());
            printf(" ReleaseSessions Size: %lld\n", server->GetReleaseSessions());

            for (int i = 1; i <= server->m_WorkThreadCnt; i++)
            {
                printf(" Send TPS : %lld\n", arrTPS[i]);
            }
            printf(" ==================================\n");
        }
    }

    return 0;
}

CTestServer::CTestServer()

{
    InitializeSRWLock(&srw_ContentQ);

    m_ContentsEvent = CreateEvent(nullptr, false, false, nullptr);
    m_ServerOffEvent = CreateEvent(nullptr, false, false, nullptr);
}

CTestServer::~CTestServer()
{
}

BOOL CTestServer::Start(const wchar_t *bindAddress, short port, int ZeroCopy, int WorkerCreateCnt, int maxConcurrency, int useNagle, int maxSessions)
{
    BOOL retval;

    m_maxSessions = maxSessions;

    retval = CLanServer::Start(bindAddress, port, ZeroCopy, WorkerCreateCnt, maxConcurrency, useNagle, maxSessions);
    hContentsThread = (HANDLE)_beginthreadex(nullptr, 0, ContentsThread, this, 0, nullptr);
    hMonitorThread = (HANDLE)_beginthreadex(nullptr, 0, MonitorThread, this, 0, nullptr);

    return retval;
}

double CTestServer::OnRecv(ull sessionID, CMessage *msg)
{
    // double CurrentQ;
    static ll visited = 0;
    ringBufferSize ContentsUseSize;

    stSRWLock srw(&srw_ContentQ);

    ContentsUseSize = m_ContentsQ.GetUseSize();
    if (msg == nullptr)
    {
        SetEvent(m_ContentsEvent);
        return double(ContentsUseSize) / double(CTestServer::s_ContentsQsize) * 100.f;
    }

    {
        Profiler profile;

        profile.Start(L"ContentsQ_Enqueue");

        // �����͸� �ְ�
        CMessage **ppMsg;
        ppMsg = &msg;

        if (m_ContentsQ.Enqueue(&sessionID, sizeof(sessionID)) != sizeof(sessionID))
            __debugbreak();

        if (m_ContentsQ.Enqueue(ppMsg, sizeof(msg)) != sizeof(msg))
            __debugbreak();

        profile.End(L"ContentsQ_Enqueue");
        SetEvent(m_ContentsEvent);
    }

    return double(ContentsUseSize) / double(CTestServer::s_ContentsQsize) * 100.f;
}

bool CTestServer::OnAccept(ull SessionID)
{

    static char *LoginPacket = CreateLoginMessage();

    WSABUF wsabuf;
    int send_retval;
    DWORD LastError;
    ull local_IoCount;

    clsSession &session = sessions_vec[(SessionID & SESSION_IDX_MASK) >> 47];
    ull idx = (SessionID & SESSION_IDX_MASK) >> 47;
    wsabuf.buf = LoginPacket;
    wsabuf.len = 10;

    _InterlockedExchange(&session.m_flag, 1);

    local_IoCount = InterlockedIncrement(&session.m_ioCount);

    CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                   L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                   L"OnAccept",
                                   L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx,
                                   L"IO_Count", local_IoCount);

    ZeroMemory(&session.m_sendOverlapped, sizeof(OVERLAPPED));

    send_retval = WSASend(session.m_sock, &wsabuf, 1, nullptr, 0, (OVERLAPPED *)&session.m_sendOverlapped, nullptr);
    CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                   L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                   L"WSASend",
                                   L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                   L"IO_Count", local_IoCount);
    LastError = GetLastError();

    if (send_retval < 0)
    {
        WSASendError(LastError, SessionID);
    }

    return true;
}

void CTestServer::EchoProcedure(ull sessionID, CMessage *message)
{
    ull SeqID;
    clsSession &session = sessions_vec[ sessionID >> 47];
    ull exchagRetval;

    // TODO : disconnect�� ����ؼ� Ǯ�� ��������.
    //
    // 

    if (InterlockedCompareExchange(&session.m_Useflag, 1, 0) != 0)
    {
        // �̹� ������ ���� ������ �޼����� �� ����.
        // ������ ������¸� '0'�� �ƴѰ��� �ǹ��ϹǷ� ���� '0'�� �ƴ� �����̶�� �����ִ� �������� �Ǵ��Ͽ� ����.

        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                       L"%-10s %10s %05lld %10s %05lld  %10s %012llu  %10s %4llu",
                                       L"EchoProcedure",
                                       L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                       L"IO_Count", session.m_ioCount);
        stTlsObjectPool<CMessage>::Release(message);

        return;
    }
    // ���� �� �����  ����� IOCount�� '1'��  ���� �ٸ� ID�� �������� ���εǵ� ����� ���Ⱑ��.
    if (InterlockedCompareExchange(&session.m_ioCount, (ull)1 << 47, 0) == 0)
    {
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                       L"ContetnsThread",
                                       L"1<<47 : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                       L"IO_Count", session.m_ioCount);

        //���⼭�� ���� session�� IO�� 0�̹Ƿ� ���� session�� ������Ѵ�.
        SeqID = session.m_SeqID.SeqNumberAndIdx;
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                       L"ContentsRelease1",
                                       L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                       L"IO_Count", session.m_ioCount);
        stTlsObjectPool<CMessage>::Release(message);
        ReleaseSession(SeqID);
        return;
    }
    // TODO : �ε����� ���� �ٸ� Session
    if (sessionID != session.m_SeqID.SeqNumberAndIdx)
    {
        // ��
        // CMessagePoolManager::pool.Release(message);
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::ERROR_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %4llu  %10s %3llu",
                                       L"idxCompareDiff",
                                       L"HANDLE : ", session.m_sock,
                                       L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                       L"Current_seqID :", sessionID,
                                       L"IO_Count", session.m_ioCount);

        if (InterlockedCompareExchange(&session.m_Useflag, 0,1) == 2)
        {
            //�׻��̿� WorkerThread�� Release ������ ��Ҵٸ�. ���⼭ ����.
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                           L"%-10s %10s %d",
                                           L"EchoProcedure2",
                                           L"UseFlag : ", 0);

            SeqID = session.m_SeqID.SeqNumberAndIdx;
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                           L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                           L"ContentsRelease5",
                                           L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                           L"IO_Count", session.m_ioCount);
        
            ReleaseSession(SeqID);
        }
        stTlsObjectPool<CMessage>::Release(message);
        return;
    }

    //_interlockedbittestandreset64
    //if ((session.m_ioCount & (ull)1 << 47) != 0)
    if (InterlockedCompareExchange(&session.m_ioCount, (ull)1 << 47, 0) == 0)
    {
        // �ߴ� ��찡 ������?
        // ����.
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                       L"ContetnsThread2",
                                       L"1<<47 : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                       L"IO_Count", session.m_ioCount);
        //������ Use�� �����ֱ� ������ �ٸ� ��������������?
        stTlsObjectPool<CMessage>::Release(message);
        ReleaseSession(sessionID);
        return;
    }
    CMessage **ppMsg;
    ull local_IoCount;
    ppMsg = &message;
    {
        {
            Profiler profile;
            profile.Start(L"LFQ_Push");

            session.m_sendBuffer.Push(*ppMsg);
            profile.End(L"LFQ_Push");
        }
    }

    if (InterlockedCompareExchange(&session.m_flag, 1, 0) == 0)
    {
        ZeroMemory(&session.m_sendOverlapped, sizeof(OVERLAPPED));
        local_IoCount = InterlockedIncrement(&session.m_ioCount);
        if ((local_IoCount & (ull)1 << 47) == 0 && local_IoCount != 1)
        {
            PostQueuedCompletionStatus(m_hIOCP, 0, (ULONG_PTR)&session, &session.m_sendOverlapped);
        }
        else
        {
            local_IoCount = InterlockedDecrement(&session.m_ioCount);
            if (InterlockedCompareExchange(&session.m_ioCount, (ull)1 << 47, 0) == 0)
            {
                CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                               L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                               L"ContetnsThread3",
                                               L"1<<47 : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                               L"IO_Count", session.m_ioCount);

                CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                               L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                               L"ContentsRelease4",
                                               L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                               L"IO_Count", session.m_ioCount);

                ReleaseSession(sessionID);
                return;
            }
        }
    }

    if (InterlockedCompareExchange(&session.m_Useflag, 0,1) == 2)
    {

        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                       L"ContentsRelease3",
                                       L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                       L"IO_Count", session.m_ioCount);
        ReleaseSession(sessionID);
    }

}
