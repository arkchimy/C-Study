#include "CNetwork_lib.h"

#include "../../../_4Course/_lib/CTlsObjectPool_lib/CTlsObjectPool_lib.h"
#include "../Profiler_MultiThread/Profiler_MultiThread.h"

#include "stHeader.h"

#include <list>
#include <thread>

CSystemLog *CLanServer::systemLog = CSystemLog::GetInstance();
extern template PVOID stTlsObjectPool<CMessage>::Alloc();       // 암시적 인스턴스화 금지
extern template void stTlsObjectPool<CMessage>::Release(PVOID); // 암시적 인스턴스화 금지

BOOL DomainToIP(const wchar_t *szDomain, IN_ADDR *pAddr)
{
    ADDRINFOW *pAddrInfo;
    SOCKADDR_IN *pSockAddr;
    if (GetAddrInfo(szDomain, L"0", NULL, &pAddrInfo) != 0)
    {
        return FALSE;
    }
    pSockAddr = (SOCKADDR_IN *)pAddrInfo->ai_addrlen;
    *pAddr = pSockAddr->sin_addr;
    FreeAddrInfo(pAddrInfo);
    return TRUE;
}
BOOL GetLogicalProcess(DWORD &out)
{
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION *infos = nullptr;
    DWORD len;
    DWORD cnt;
    DWORD temp = 0;

    len = 0;

    GetLogicalProcessorInformation(infos, &len);

    infos = new SYSTEM_LOGICAL_PROCESSOR_INFORMATION[len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION)];

    if (infos == nullptr)
        return false;

    GetLogicalProcessorInformation(infos, &len);

    cnt = len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION); // 반복문

    for (DWORD i = 0; i < cnt; i++)
    {
        if (infos[i].Relationship == RelationProcessorCore)
        {
            ULONG_PTR mask = infos[i].ProcessorMask;
            // 논리 프로세서의 수는 set된 비트의 개수
            while (mask)
            {
                temp += (mask & 1);
                mask >>= 1;
            }
        }
    }
    printf("LogicalProcess Cnt : %d \n", temp);

    delete[] infos;
    out = temp;
    return true;
}
SRWLOCK srw_Log;

unsigned AcceptThread(void *arg)
{
    SOCKET client_sock;
    SOCKADDR_IN addr;

    size_t *arrArg = (size_t *)arg;
    HANDLE hIOCP = (HANDLE)*arrArg;
    SOCKET listen_sock = SOCKET(arrArg[1]);

    DWORD listen_retval;
    DWORD flag;

    ull session_id = 0;
    ull idx;

    clsSession *session;
    CLanServer *server;

    int addrlen;
    stSessionId stsessionID;
    ull local_IoCount;

    LONG64 localAllocCnt;
    /* arg 에 대해서
     * arg[0] 에는 클라이언트에게 연결시킬 hIOCP의 HANDLE을 넣기.
     * arg[1] 에는 listen_sock의 주소를 넣기
     */
    CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                   L"%-20s ",
                                   L"This is AcceptThread");
    CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::SYSTEM_Mode,
                                   L"%-20s ",
                                   L"This is AcceptThread");
 



    addrlen = sizeof(addr);
    server = reinterpret_cast<CLanServer *>(arrArg[2]);

    listen_retval = listen(listen_sock, SOMAXCONN_HINT(65535));
    flag = 0;

    InitializeSRWLock(&srw_Log);

    if (listen_retval == 0)
        printf("Listen Sucess\n");
    else
        ERROR_FILE_LOG(L"Socket_Error.txt", L"Listen_Error");
    while (1)
    {
        client_sock = accept(listen_sock, (sockaddr *)&addr, &addrlen);
        if (client_sock == INVALID_SOCKET)
        {
            ERROR_FILE_LOG(L"Socket_Error.txt", L"Accept Error ");
            continue;
        }
        server->arrTPS[0]++; //Accept TPS 측정

        {

            if (server->m_SessionIdxStack.m_size == 0)
            {
                closesocket(client_sock);
                // 추가적으로 구현해야되는 부분이 존재함.
                //  DisConnect 행위
                __debugbreak();
                continue;
            }
            server->m_SessionIdxStack.Pop(idx);
        }

        clsSession& session = server->sessions_vec[idx];
        stsessionID.idx = idx;
        stsessionID.seqNumber = session_id++;

        InterlockedExchange(&session.m_sock, client_sock);
        InterlockedExchange(&session.m_blive, 1);
        InterlockedExchange(&session.m_flag, 0);
        InterlockedExchange(&session.m_ioCount, 1); // 1로 시작하므로써 0으로 초기화때 Contents에서 오인하는 일을 방지.
        InterlockedExchange(&session.m_SeqID.SeqNumberAndIdx, stsessionID.SeqNumberAndIdx);


        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu\n",
                                       L"Accept", L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx);

        _interlockedincrement64(&server->m_SessionCount);
        CreateIoCompletionPort((HANDLE)client_sock, hIOCP, (ull)&session, 0);

        // AllocMsg의 처리가 너무 많이 발생한다면 False를 반환.
        if (server->OnAccept(session.m_SeqID.SeqNumberAndIdx) == false)
        {
            server->DecrementIoCountAndMaybeDeleteSession(session);
            continue;
        }

        server->RecvPacket(session);

        server->DecrementIoCountAndMaybeDeleteSession(session);       
    }
    return 0;
}

unsigned WorkerThread(void *arg)
{
    /*
     * arg[0] 에는 hIOCP의 가짜핸들을
     * arg[1] 에는 Server의 인스턴스
     */

    CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                   L"%-20s ",
                                   L"This is WorkerThread");
    CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::SYSTEM_Mode,
                                   L"%-20s ",
                                   L"This is WorkerThread");
    size_t *arrArg = reinterpret_cast<size_t *>(arg);
    HANDLE hIOCP = (HANDLE)*arrArg;
    CLanServer *server = reinterpret_cast<CLanServer *>(arrArg[1]);

    static LONG64 s_arrTPS_idx = 0;
    LONG64 *arrTPS_idx = reinterpret_cast<LONG64 *>(TlsGetValue(server->m_tlsIdxForTPS));
    // arrTPS 의 index 값을 셋팅함.
    if (arrTPS_idx == nullptr)
    {
        TlsSetValue(server->m_tlsIdxForTPS, (void *)_interlockedincrement64(&s_arrTPS_idx));
    }

    DWORD transferred;
    ull key;
    OVERLAPPED *overlapped;
    clsSession *session; // 특정 Msg를 목적으로 nullptr을 PQCS하는 경우가 존재.

    ull local_IoCount;
    Profiler profile;

    while (server->bOn)
    {
        // 지역변수 초기화
        {
            transferred = 0;
            key = 0;
            overlapped = nullptr;
            session = nullptr;
        }

        GetQueuedCompletionStatus(hIOCP, &transferred, &key, &overlapped, INFINITE);

        if (overlapped == nullptr)
        {
            ERROR_FILE_LOG(L"Socket_Error.txt", L"GetQueuedCompletionStatus Overlapped is nullptr");
            continue;
        }
        session = reinterpret_cast<clsSession *>(key);
        // TODO : JumpTable 생성 되는 지?
        switch (reinterpret_cast<stOverlapped *>(overlapped)->_mode)
        {
        case Job_Type::Recv:
            //// FIN 의 경우에
            // if (transferred == 0)
            //     break;
            server->RecvComplete(*session, transferred);
            break;
        case Job_Type::Send:
            server->SendComplete(*session, transferred);
            break;

        case Job_Type::MAX:
            ERROR_FILE_LOG(L"Socket_Error.txt", L"UnDefine Error Overlapped_mode");
            __debugbreak();
        }
        local_IoCount = InterlockedDecrement(&session->m_ioCount);

        switch (reinterpret_cast<stOverlapped *>(overlapped)->_mode)
        {
        case Job_Type::Recv:
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                           L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                           L"RecvComplete",
                                           L"HANDLE : ", session->m_sock, L"seqID :", session->m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session->m_SeqID.idx,
                                           L"IO_Count", local_IoCount);
            break;
        case Job_Type::Send:
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                           L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                           L"SendComplete",
                                           L"HANDLE : ", session->m_sock, L"seqID :", session->m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session->m_SeqID.idx,
                                           L"IO_Count", local_IoCount);
        }

        if (local_IoCount == 0)
        {
            ull compareRetval = InterlockedCompareExchange(&session->m_ioCount, (ull)1 << 47, 0);
            if (compareRetval != 0)
            {

                CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                               L"%-10s  %-10s  %llu %10s %05lld  %10s %018llu  %10s %4llu ",
                                               L"1 << 47Faild", L"retval : ", compareRetval,
                                               L"HANDLE : ", session->m_sock, L"seqID :", session->m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session->m_SeqID.idx);

                continue;
            }
            CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                           L"%-10s %10s %05lld  %10s %012llu  %10s %4llu ",
                                           L"WorkerRelease",
                                           L"HANDLE : ", session->m_sock, L"seqID :", session->m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session->m_SeqID.idx);
            ull seqID = session->m_SeqID.SeqNumberAndIdx;
            server->ReleaseSession(seqID);
        }
    }
    printf("WorkerThreadID : %d  return '0' \n", GetCurrentThreadId());

    return 0;
}

CLanServer::CLanServer(bool EnCoding)
    : bEnCording(EnCoding)
{
    if (bEnCording)
        headerSize = sizeof(stHeader);
    else
        headerSize =  offsetof(stHeader, sDataLen);

    m_listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (m_listen_sock == INVALID_SOCKET)
    {

        ERROR_FILE_LOG(L"Socket_Error.txt",
                       L"listen_sock Create Socket Error");
        __debugbreak();
    }
}
CLanServer::~CLanServer()
{
    closesocket(m_listen_sock);
}

BOOL CLanServer::Start(const wchar_t *bindAddress, short port, int ZeroCopy, int WorkerCreateCnt, int reduceThreadCount, int noDelay, int MaxSessions)
{
    linger linger;
    int buflen;
    DWORD lProcessCnt;
    DWORD bind_retval;

    SOCKADDR_IN serverAddr;

    m_tlsIdxForTPS = TlsAlloc();
    if (m_tlsIdxForTPS == TLS_OUT_OF_INDEXES)
        __debugbreak();

    sessions_vec.resize(MaxSessions);

    for (ull idx = 0; idx < MaxSessions; idx++)
        m_SessionIdxStack.Push(idx);

    linger.l_onoff = 1;
    linger.l_linger = 0;

    ZeroMemory(&serverAddr, sizeof(serverAddr));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    InetPtonW(AF_INET, bindAddress, &serverAddr.sin_addr);

    if (ZeroCopy)
    {
        bZeroCopy = true;
        buflen = 0;
        setsockopt(m_listen_sock, SOL_SOCKET, SO_SNDBUF, (const char *)&buflen, sizeof(buflen));
        getsockopt(m_listen_sock, SOL_SOCKET, SO_SNDBUF, (char *)&buflen, &buflen);
        printf("getsockopt SendBuffer Len : %d \n", buflen);
    }

    setsockopt(m_listen_sock, SOL_SOCKET, SO_LINGER, (const char *)&linger, sizeof(linger));
    setsockopt(m_listen_sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&noDelay, sizeof(noDelay));

    bind_retval = bind(m_listen_sock, (sockaddr *)&serverAddr, sizeof(serverAddr));
    if (bind_retval != 0)
        ERROR_FILE_LOG(L"Socket_Error.txt", L"Bind Failed");

    if (GetLogicalProcess(lProcessCnt) == false)
        ERROR_FILE_LOG(L"GetLogicalProcessError.txt", L"GetLogicalProcess_Error");

    m_hIOCP = (HANDLE)CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, NULL, lProcessCnt - reduceThreadCount);

    m_hThread = new HANDLE[WorkerCreateCnt];
    arrTPS = new LONG64[WorkerCreateCnt + 1];
    ZeroMemory(arrTPS, sizeof(LONG64) * (WorkerCreateCnt + 1));

    m_WorkThreadCnt = WorkerCreateCnt;

    {
        WorkerArg[0] = m_hIOCP;
        WorkerArg[1] = this;

        AcceptArg[0] = m_hIOCP;
        AcceptArg[1] = (HANDLE)m_listen_sock;
        AcceptArg[2] = this;
    }

    bOn = true;

    m_hAccept = (HANDLE)_beginthreadex(nullptr, 0, AcceptThread, &AcceptArg, 0, nullptr);
    for (int i = 0; i < WorkerCreateCnt; i++)
        m_hThread[i] = (HANDLE)_beginthreadex(nullptr, 0, WorkerThread, &WorkerArg, 0, nullptr);

    return true;
}

void CLanServer::Stop()
{
}

bool CLanServer::Disconnect(const ull SessionID)
{
    // WorkerThread에서 호출하는 DisConnect이므로  IO가 '0' 이 되는 경우의 수가 없음.
    clsSession &session = sessions_vec[SessionID >> 47];

    ull Local_ioCount;
    ull seqID;

    // session의 보장 장치.
    Local_ioCount = InterlockedIncrement(&session.m_ioCount);

    if ((Local_ioCount & (ull)1 << 47) != 0)
    {
        return false;
    }

    if (SessionID != session.m_SeqID.SeqNumberAndIdx)
    {

        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                       L"%-10s %10s %05lld  %10s %018llu  %10s %4llu %10s %018llu  %10s %4llu ",
                                       L"Disconnect",
                                       L"HANDLE : ", session.m_sock,
                                       L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                       L"LocalseqID :", SessionID, L"LocalseqIndx : ", SessionID >> 47);
        return false;
    }

    ull retval = InterlockedExchange(&session.m_blive, 0);
    if (retval == 1)
    {
        InterlockedIncrement(&iDisCounnectCount);
    }
    CancelIO_Routine(SessionID);
    // ContentsThread가 호출하게되면, 연결이 끊긴 Session의 ID가 올 수 있다.

    Local_ioCount = InterlockedDecrement(&session.m_ioCount);
    // 앞으로 Session 초기화는 IoCount를 '0'으로 하면 안된다.
    if (InterlockedCompareExchange(&session.m_ioCount, (ull)1 << 47, 0) == 0)
        ReleaseSession(SessionID);
    return true;
}

void CLanServer::CancelIO_Routine(const ull SessionID)
{
    // Session에 대한 안정성은  외부에서 보장해주세요.
    BOOL retval;
    clsSession &session = sessions_vec[SessionID >> 47];

    retval = CancelIoEx((HANDLE)session.m_sock, &session.m_sendOverlapped);
    retval = CancelIoEx((HANDLE)session.m_sock, &session.m_recvOverlapped);
}

LONG64 CLanServer::GetSessionCount()
{
    // AcceptThread에서 전담한다면, interlock이 필요없다.
    return m_SessionCount;
}

LONG64 CLanServer::GetReleaseSessions()
{
    return m_ReleaseSessions.m_size;
}

LONG64 CLanServer::Get_IdxStack()
{
    return m_SessionIdxStack.m_size;
}

void CLanServer::RecvComplete(clsSession &session, DWORD transferred)
{
    stHeader header;
    ringBufferSize useSize,deQsize;
    float qPersentage;
    ull SeqID;
    char *f, *r, *b;

    {
        session.m_recvBuffer.MoveRear(transferred);

        SeqID = session.m_SeqID.SeqNumberAndIdx;

        
        b = session.m_recvBuffer._begin;
        f = session.m_recvBuffer._frontPtr;
        r = session.m_recvBuffer._rearPtr;

        // ContentsQ의 상황이 어떤지를 체크.
        qPersentage = OnRecv(SeqID, nullptr);
        if (qPersentage >= 75.f)
        {
            Disconnect(SeqID);
            CSystemLog::GetInstance()->Log(L"Disconnect", en_LOG_LEVEL::ERROR_Mode,
                                           L"%-20s %10s %05f %10s %08llu",
                                           L"ContentsQ Full",
                                           L"qPersentage : ", qPersentage,
                                           L"TargetID : ", SeqID);
            return;
        }
    
    }
    // Header의 크기만큼을 확인.
   
    while (session.m_recvBuffer.Peek(&header, headerSize, f, r) == headerSize)
    {
        useSize = session.m_recvBuffer.GetUseSize(f, r);
        if (useSize < header.sDataLen + headerSize)
        {
            // 데이터가 덜 옴.
            break;
        }
        // 메세지 생성
        CMessage *msg = CreateMessage(session, header);

        if (msg == nullptr)
            break;
        if (bEnCording)
            msg->DeCoding();

        msg->_frontPtr = msg->_frontPtr + headerSize;
        // PayLoad를 읽고 무엇인가 처리하는 Logic이 NetWork에 들어가선 안된다.

        {
            Profiler profile;
            profile.Start(L"OnRecv");
            qPersentage = OnRecv(SeqID, msg);
            profile.End(L"OnRecv");
        }

        if (qPersentage >= 75.0)
        {
            Disconnect(SeqID);
            CSystemLog::GetInstance()->Log(L"Disconnect", en_LOG_LEVEL::ERROR_Mode,
                                           L"%-20s %10s %05f %10s %08llu",
                                           L"ContentsQ Full",
                                           L"qPersentage : ", qPersentage,
                                           L"TargetID : ", SeqID);
            return ;
        }

        f = session.m_recvBuffer._frontPtr;
        r = session.m_recvBuffer._rearPtr;
    }

    RecvPacket(session);
}

void CLanServer::DecrementIoCountAndMaybeDeleteSession(clsSession &session)
{
    LONG64 local_IoCount;

    local_IoCount = InterlockedDecrement(&session.m_ioCount);
    if (local_IoCount == 0)
    {

        if (InterlockedCompareExchange(&session.m_ioCount, (ull)1 << 47, 0) != 0)
            return;
        
        ReleaseSession(session.m_SeqID.SeqNumberAndIdx);
    }
}

CMessage *CLanServer::CreateMessage(clsSession &session, class stHeader &header)
{
    // TODO : Header를 읽고, 생성하고

    CMessage *msg;
    ringBufferSize deQsize;

    {
        Profiler profile;
        profile.Start(L"PoolAlloc");
        msg = reinterpret_cast<CMessage *>(stTlsObjectPool<CMessage>::Alloc());
        profile.End(L"PoolAlloc");
    }
    // 순수하게 데이터만 가져옴.  EnCording 의 경우 RandKey와 CheckSum도 가져옴.
    deQsize = session.m_recvBuffer.Dequeue(msg->_frontPtr, header.sDataLen + headerSize);
    msg->_rearPtr = msg->_frontPtr + deQsize;

    if (header.sDataLen + headerSize != deQsize)
    {
        // TODO : 조작된 패킷
        CSystemLog::GetInstance()->Log(L"Disconnect", en_LOG_LEVEL::ERROR_Mode,
                                       L"%-20s %10s %05d %10s %05d",
                                       L"riggedPacket",
                                       L"DequeueSize : ", sizeof(stHeader),
                                       L"Retval : ", deQsize);
        msg->HexLog();
        return nullptr;
    }
    if (_interlockedexchange64(&msg->iUseCnt, 1) != 0)
        __debugbreak();
    return msg;
}

char *CLanServer::CreateLoginMessage()
{
    static short header = 0x0008;
    static ull PayLoad = 0x7fffffffffffffff;
    static char msg[10];

    memcpy(msg, &header, sizeof(header));
    memcpy(msg + sizeof(header), &PayLoad, sizeof(PayLoad));

    return msg;
}


void CLanServer::SendComplete(clsSession &session, DWORD transferred)
{
    ringBufferSize useSize, directDQSize;

    DWORD bufCnt;
    int send_retval = 0;

    WSABUF wsaBuf[500];
    DWORD LastError;
    // LONG64 beforeTPS;
    LONG64 TPSidx;
    ull local_IoCount;
    CMessage *msg;

    while (session.m_SendMsg.Pop(msg))
    {
        stTlsObjectPool<CMessage>::Release(msg);
    }

    useSize = (ringBufferSize)session.m_sendBuffer.m_size;
    TPSidx = (LONG64)TlsGetValue(m_tlsIdxForTPS);

    if (useSize == 0)
    {
        useSize = (ringBufferSize)session.m_sendBuffer.m_size;
        // flag 끄기
        if (_InterlockedCompareExchange(&session.m_flag, 0, 1) == 1)
        {
            useSize = (ringBufferSize)session.m_sendBuffer.m_size;
            if (useSize != 0)
            {
                //// 누군가 진입 했다면 return
                if (_InterlockedCompareExchange(&session.m_flag, 1, 0) == 0)
                {
                    ZeroMemory(&session.m_sendOverlapped, sizeof(OVERLAPPED));
                    InterlockedIncrement(&session.m_ioCount);
                    PostQueuedCompletionStatus(m_hIOCP, 0, (ULONG_PTR)&session, &session.m_sendOverlapped);
                }
            }
        }

        return;
    }

    {
        ZeroMemory(wsaBuf, sizeof(wsaBuf));
        ZeroMemory(&session.m_sendOverlapped, sizeof(OVERLAPPED));
    }

    ull msgAddr;
    bufCnt = 0;

    Profiler profile;
    profile.Start(L"LFQ_Pop");
    while (session.m_sendBuffer.Pop(msg))
    {

        session.m_SendMsg.Push(msg);

        wsaBuf[bufCnt].buf = msg->_frontPtr;
        wsaBuf[bufCnt].len = SerializeBufferSize(msg->_rearPtr - msg->_frontPtr);

        bufCnt++;
    }
    profile.End(L"LFQ_Pop");

    arrTPS[TPSidx] += bufCnt;

    if (session.m_blive)
    {
        local_IoCount = _InterlockedIncrement(&session.m_ioCount);

        if (bZeroCopy)
        {
            Profiler::Start(L"ZeroCopy WSASend");
            send_retval = WSASend(session.m_sock, wsaBuf, bufCnt, nullptr, 0, (OVERLAPPED *)&session.m_sendOverlapped, nullptr);
            LastError = GetLastError();
            Profiler::End(L"ZeroCopy WSASend");
        }
        else
        {
            Profiler::Start(L"WSASend");

            send_retval = WSASend(session.m_sock, wsaBuf, bufCnt, nullptr, 0, (OVERLAPPED *)&session.m_sendOverlapped, nullptr);
            LastError = GetLastError();
            Profiler::End(L"WSASend");
        }

        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                       L"WSASend",
                                       L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                       L"IO_Count", local_IoCount);
        if (send_retval < 0)
            WSASendError(LastError, session.m_SeqID.SeqNumberAndIdx);
    }
}
bool CLanServer::SessionLock(ull SessionID) 
{
    /*
        ※ 인자로 Msg들고오기 싫음. 
        => False를 리턴받는다면 호출부에서 msg폐기를 요구합니다.

        * Release된 Session이라면  False를 리턴  
        * SessionID가 다르다면     False를 리턴  IO를 감소 시킨후 Release시도 ,  적어도 진입순간에는 Session은 살아있었음.
        * 증가시킨 IO가 '1'인 경우 감소 시킨 후  Release시도 , False를 리턴
        * 
    */
    clsSession& session = sessions_vec[SessionID >> 47];
    ull Local_ioCount;
    ull seqID;

    // session의 보장 장치.
    Local_ioCount = InterlockedIncrement(&session.m_ioCount);

    if ((Local_ioCount & (ull)1 << 47) != 0)
    {
        // 새로운 세션으로 초기화되지않았고, r_flag가 세워져있으면 진입하지말자.
        // 이미 r_flag가 올라가있는데 IoCount를 잘못 올린다고 문제가 되지않을 것같다.
        return false;
    }

    // session의 Release는 막았으므로 변경되지않음.
    seqID = session.m_SeqID.SeqNumberAndIdx;
    if (seqID != SessionID)
    {
        // 새로 세팅된 Session이므로 다른 연결이라 판단.
        // 내가 잘못 올린 ioCount를 감소 시켜주어야한다.
        Local_ioCount = InterlockedDecrement(&session.m_ioCount);
        // 앞으로 Session 초기화는 IoCount를 '0'으로 하면 안된다.

        if (InterlockedCompareExchange(&session.m_ioCount, (ull)1 << 47, 0) == 0)
            ReleaseSession(seqID);

        return false;
    }

    if (Local_ioCount == 1)
    {
        // 원래 '0'이 었는데 내가 증가시켰다.
        Local_ioCount = InterlockedDecrement(&session.m_ioCount);
        // 앞으로 Session 초기화는 IoCount를 '0'으로 하면 안된다.

        if (InterlockedCompareExchange(&session.m_ioCount, (ull)1 << 47, 0) == 0)
            ReleaseSession(SessionID);

        return false;
    }
    return true;
}
void CLanServer::SessionUnLock(ull SessionID) 
{
    clsSession &session = sessions_vec[SessionID >> 47];
    ull Local_ioCount;

    Local_ioCount = InterlockedDecrement(&session.m_ioCount);
    // 앞으로 Session 초기화는 IoCount를 '0'으로 하면 안된다.
    if (InterlockedCompareExchange(&session.m_ioCount, (ull)1 << 47, 0) == 0)
        ReleaseSession(SessionID);
}

void CLanServer::SendPacket(ull SessionID, CMessage *msg, BYTE SendType,
                            int iSectorX, int iSectorY)
{
    switch (SendType)
    {
    case 0:
        UnitCast(SessionID, msg);
        break;
    //case 1:
    //    BroadCast(SessionID, msg, iSectorX, iSectorY);
    //    break;
    }
      
}
void CLanServer::UnitCast(ull SessionID, CMessage *msg)
{
    clsSession &session = sessions_vec[SessionID >> 47];
    ull Local_ioCount;
    ull seqID;
    // session의 보장 장치.
    Local_ioCount = InterlockedIncrement(&session.m_ioCount);

    if ((Local_ioCount & (ull)1 << 47) != 0)
    {
        // 새로운 세션으로 초기화되지않았고, r_flag가 세워져있으면 진입하지말자.
        stTlsObjectPool<CMessage>::Release(msg);
        // 이미 r_flag가 올라가있는데 IoCount를 잘못 올린다고 문제가 되지않을 것같다.
        return;
    }

    // session의 Release는 막았으므로 변경되지않음.
    seqID = session.m_SeqID.SeqNumberAndIdx;
    if (seqID != SessionID)
    {
        // 새로 세팅된 Session이므로 다른 연결이라 판단.
        stTlsObjectPool<CMessage>::Release(msg);
        // 내가 잘못 올린 ioCount를 감소 시켜주어야한다.
        Local_ioCount = InterlockedDecrement(&session.m_ioCount);
        // 앞으로 Session 초기화는 IoCount를 '0'으로 하면 안된다.

        if (InterlockedCompareExchange(&session.m_ioCount, (ull)1 << 47, 0) == 0)
            ReleaseSession(seqID);

        return;
    }

    if (Local_ioCount == 1)
    {
        // 원래 '0'이 었는데 내가 증가시켰다.
        Local_ioCount = InterlockedDecrement(&session.m_ioCount);
        // 앞으로 Session 초기화는 IoCount를 '0'으로 하면 안된다.

        if (InterlockedCompareExchange(&session.m_ioCount, (ull)1 << 47, 0) == 0)
            ReleaseSession(SessionID);

        stTlsObjectPool<CMessage>::Release(msg);
        return;
    }

    // 여기까지 왔다면, 같은 Session으로 판단하자.
    CMessage **ppMsg;
    ull local_IoCount;
    ppMsg = &msg;

    {
        Profiler profile;
        profile.Start(L"LFQ_Push");

        session.m_sendBuffer.Push(*ppMsg);
        profile.End(L"LFQ_Push");
    }

    // PQCS를 시도.
    if (InterlockedCompareExchange(&session.m_flag, 1, 0) == 0)
    {
        ZeroMemory(&session.m_sendOverlapped, sizeof(OVERLAPPED));
        local_IoCount = InterlockedIncrement(&session.m_ioCount);

        PostQueuedCompletionStatus(m_hIOCP, 0, (ULONG_PTR)&session, &session.m_sendOverlapped);
    }

    Local_ioCount = InterlockedDecrement(&session.m_ioCount);
    // 앞으로 Session 초기화는 IoCount를 '0'으로 하면 안된다.
    if (InterlockedCompareExchange(&session.m_ioCount, (ull)1 << 47, 0) == 0)
        ReleaseSession(SessionID);
}
void CLanServer::BroadCast(ull SessionID, CMessage *msg , WORD SectorX, WORD SectorY)
{

}
void CLanServer::RecvPacket(clsSession &session)
{
    ringBufferSize freeSize;
    ringBufferSize directEnQsize;

    int wsaRecv_retval = 0;
    DWORD LastError;
    DWORD flag = 0;
    DWORD bufCnt;
    ull local_IoCount;

    WSABUF localRecvWSABuf[2]{0};

    char *f = session.m_recvBuffer._frontPtr, *r = session.m_recvBuffer._rearPtr;

    {
        ZeroMemory(&session.m_recvOverlapped, sizeof(OVERLAPPED));
    }

    directEnQsize = session.m_recvBuffer.DirectEnqueueSize(f, r);
    freeSize = session.m_recvBuffer.GetFreeSize(f, r); // SendBuffer에 바로넣기 위함.
    if (freeSize < directEnQsize)
        __debugbreak();
    if (freeSize <= directEnQsize)
    {
        localRecvWSABuf[0].buf = r;
        localRecvWSABuf[0].len = directEnQsize;

        bufCnt = 1;
    }
    else
    {
        localRecvWSABuf[0].buf = r;
        localRecvWSABuf[0].len = directEnQsize;

        localRecvWSABuf[1].buf = session.m_recvBuffer._begin;
        localRecvWSABuf[1].len = freeSize - directEnQsize;

        bufCnt = 2;
    }

    if (session.m_blive)
    {
        local_IoCount = _InterlockedIncrement(&session.m_ioCount);
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                       L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                       L"WSARecv",
                                       L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                       L"IO_Count", local_IoCount);
        wsaRecv_retval = WSARecv(session.m_sock, localRecvWSABuf, bufCnt, nullptr, &flag, &session.m_recvOverlapped, nullptr);

        LastError = GetLastError();
        if (wsaRecv_retval < 0)
            WSARecvError(LastError, session.m_SeqID.SeqNumberAndIdx);
    }
}

int CLanServer::getAcceptTPS()
{
    return 0;
}
int CLanServer::getRecvMessageTPS()
{
    return 0;
}
int CLanServer::getSendMessageTPS()
{
    return 0;
}


void CLanServer::WSASendError(const DWORD LastError, const ull SessionID)
{
    clsSession &session = sessions_vec[SessionID >> 47];
    ull local_IoCount;
    // TODO : JumpTable이 만들어지는가?
    switch (LastError)
    {
    case WSA_IO_PENDING:
        if (session.m_blive == 0)
        {
            CancelIoEx((HANDLE)session.m_sock, &session.m_sendOverlapped);
        }
        break;

    case WSAEINTR: // 10004
        session.m_blive = 0;
        local_IoCount = _InterlockedDecrement(&session.m_ioCount);
        break;
    case WSAENOTSOCK:     // 10038
    case WSAECONNABORTED: //    10053 :
    case WSAECONNRESET:   // 10054:
        session.m_blive = 0;
        local_IoCount = _InterlockedDecrement(&session.m_ioCount);
        break;

    default:

        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::ERROR_Mode,
                                       L"[ %-10s %05d ],%10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                       L"Send_UnDefineError", LastError,
                                       L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx,
                                       L"IO_Count", session.m_ioCount);
        session.m_blive = 0;
        local_IoCount = _InterlockedDecrement(&session.m_ioCount);
    }
}

void CLanServer::WSARecvError(const DWORD LastError, const ull SessionID)
{
    clsSession &session = sessions_vec[SessionID >> 47];
    ull local_IoCount;
    DWORD CancelRetval;

    // TODO : JumpTable이 만들어지는가?
    switch (LastError)
    {
    case WSA_IO_PENDING:
        if (session.m_blive == 0)
        {
            CancelIoEx((HANDLE)session.m_sock, &session.m_recvOverlapped);
        }
        break;

    case WSAEINTR:        // 10004
    case WSAENOTSOCK:     // 10038
    case WSAECONNABORTED: //    10053 :
    case WSAECONNRESET:   // 10054:
        session.m_blive = 0;
        local_IoCount = _InterlockedDecrement(&session.m_ioCount);
        break;

    default:
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::ERROR_Mode,
                                       L"[ %-10s %05d ] %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                       L"Recv_UnDefineError", LastError,
                                       L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx,
                                       L"IO_Count", session.m_ioCount);
        session.m_blive = 0;
        local_IoCount = _InterlockedDecrement(&session.m_ioCount);
    }
}

void CLanServer::ReleaseSession(ull SessionID)
{
    int retval;

    clsSession &session = sessions_vec[SessionID >> 47];
    if (SessionID != session.m_SeqID.SeqNumberAndIdx)
    {
        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                       L"%-10s %10s %05lld  %10s %018llu  %10s %4llu %10s %018llu  %10s %4llu ",
                                       L"ReleaseSessionNoequle",
                                       L"HANDLE : ", session.m_sock,
                                       L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                       L"LocalseqID :", SessionID, L"LocalseqIndx : ", SessionID >> 47);
        __debugbreak();
        return;
    }
    CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::DEBUG_Mode,
                                   L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                   L"Closesocket",
                                   L"HANDLE : ", session.m_sock, L"seqID :", session.m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session.m_SeqID.idx,
                                   L"IO_Count", session.m_ioCount);
    OnRelease(SessionID);
    session.Release();
    retval = closesocket(session.m_sock);
    DWORD LastError = GetLastError();
    if (retval != 0)
    {

        CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::ERROR_Mode,
                                       L"[ %-10s %05d ],%10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                       L"closesocketError", LastError,
                                       L"HANDLE : ", session.m_sock, L"seqID :", SessionID, L"seqIndx : ", session.m_SeqID.idx,
                                       L"IO_Count", session.m_ioCount);
    }
    m_SessionIdxStack.Push(SessionID >> 47);
    _interlockeddecrement64(&m_SessionCount);
   
}

