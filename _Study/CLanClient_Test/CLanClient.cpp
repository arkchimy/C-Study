#include "CLanClient.h"

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
    delete[] infos;
    out = temp;
    return true;
}

CLanClient::CLanClient(bool EnCoding )
    : bEnCording(EnCoding)
{    
        if (bEnCording)
        headerSize = sizeof(stHeader);
    else
        headerSize = offsetof(stHeader, sDataLen);
}

void CLanClient::WorkerThread()
{

    DWORD transferred;
    unsigned long long  key;
    OVERLAPPED *overlapped;
    clsSession *session; // 특정 Msg를 목적으로 nullptr을 PQCS하는 경우가 존재.
    ull local_IoCount;

    while (1)
    {
        {
            transferred = 0;
            key = 0;
            overlapped = nullptr;
            session = nullptr;
        }
        GetQueuedCompletionStatus(m_hIOCP, &transferred, &key, &overlapped, INFINITE);
        // transferred 이 0 인 경우에는 상대방이 FIN으로 Send를 닫았는데 WSARecv를 걸경우 0을 반환함.

        if (transferred == 0 && overlapped == nullptr && key == 0)
            break;
        if (overlapped == nullptr)
        {
            //CSystemLog::GetInstance()->Log(L"GQCS.txt", en_LOG_LEVEL::ERROR_Mode, L"GetQueuedCompletionStatus Overlapped is nullptr");
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
            RecvComplete(*session, transferred);
            break;
        case Job_Type::Send:
            SendComplete(*session, transferred);
            break;
        case Job_Type::ReleasePost:
            ReleaseComplete(key);
            OnLeaveServer(key);
            continue;
        default:
            //CSystemLog::GetInstance()->Log(L"GQCS.txt", en_LOG_LEVEL::ERROR_Mode, L"UnDefine Error Overlapped_mode : %d", reinterpret_cast<stOverlapped *>(overlapped)->_mode);
            __debugbreak();
        }
        local_IoCount = InterlockedDecrement(&session->m_ioCount);

        if (local_IoCount == 0)
        {
            ull compareRetval = InterlockedCompareExchange(&session->m_ioCount, (ull)1 << 47, 0);
            if (compareRetval != 0)
            {
                continue;
            }

            ull seqID = session->_sessionID;
            ReleaseSession(seqID);
        }
    }
}

bool CLanClient::Connect(wchar_t *ServerAddress, short Serverport, wchar_t *BindipAddress, int workerThreadCnt, int bNagle, int reduceThreadCount, int userCnt)
{
    DWORD logicalProcess;
    linger linger;
    SOCKADDR_IN serverAddr;


    // ConcurrentThread 
    {
        GetLogicalProcess(logicalProcess);
        m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, NULL, logicalProcess - reduceThreadCount);
        //Create WorkerThread 
        {
            _hIocpThreadVec.resize(workerThreadCnt);
            for (int i =0; i < workerThreadCnt ; i++)
            {
                _hIocpThreadVec[i] = std::thread(&CLanClient::WorkerThread, this);
            }
        }
    }

    _sockVec.resize(userCnt);
    for (int i = 0; i < SessionMax; i++)
        _IdxStack.push(i);

    ZeroMemory(&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(Serverport);
    InetPtonW(AF_INET, ServerAddress, &serverAddr.sin_addr);

    linger.l_onoff = 1;
    linger.l_linger = 0;

    
    
    for (int i =0; i < userCnt ; i++)
    {
        _sockVec[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (_sockVec[i] == INVALID_SOCKET)
        {
  
            __debugbreak();
        }

        setsockopt(_sockVec[i], SOL_SOCKET, SO_LINGER, (const char *)&linger, sizeof(linger));
        setsockopt(_sockVec[i], IPPROTO_TCP, TCP_NODELAY, (const char *)&bNagle, sizeof(bNagle));
        int connectRetval;
        connectRetval = connect(_sockVec[i], (const sockaddr *)&serverAddr, sizeof(serverAddr));
        if (connectRetval == SOCKET_ERROR)
        {
            DWORD lastError = GetLastError();
            __debugbreak();
        }
        ull top = _IdxStack.top();
        _IdxStack.pop();
        ull SessionID = top;

        SessionID = SessionID << 47;
        SessionID += g_ID++;

        sessions_vec[top]._sessionID = SessionID;
        sessions_vec[top]._sock = _sockVec[top];
        OnEnterJoinServer(SessionID);

        CreateIoCompletionPort((HANDLE)sessions_vec[top]._sock, m_hIOCP, (ULONG_PTR)&sessions_vec[top], 0);
    }




    return false;
}

bool CLanClient::Disconnect()
{
    // WorkerThread에서 호출하는 DisConnect이므로  IO가 '0' 이 되는 경우의 수가 없음.
    clsSession &session = sessions_vec[SessionID >> 47];

    ull Local_ioCount;

    // session의 보장 장치.
    Local_ioCount = InterlockedIncrement(&session.m_ioCount);

    if ((Local_ioCount & (ull)1 << 47) != 0)
    {
        return false;
    }

    if (SessionID != session.m_SeqID.SeqNumberAndIdx)
    {
        Local_ioCount = InterlockedDecrement(&session.m_ioCount);
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

CMessage *CLanClient::CreateMessage(clsSession &session, stHeader &header) const
{
    // TODO : Header를 읽고, 생성하고

    CMessage *msg;
    ringBufferSize deQsize;

    // 메세지 할당
    {

        {
            Profiler profile(L"PoolAlloc");
            msg = reinterpret_cast<CMessage *>(stTlsObjectPool<CMessage>::Alloc());
        }
    }
    // 순수하게 데이터만 가져옴.  EnCording 의 경우 RandKey와 CheckSum도 가져옴.
    deQsize = session.m_recvBuffer.Dequeue(msg->_frontPtr, header.sDataLen + headerSize);
    msg->_rearPtr = msg->_frontPtr + deQsize;

    if (header.sDataLen + headerSize != deQsize)
    {
        return nullptr;
    }
    return msg;
}

void CLanClient::SendPacket(ull SessionID, CMessage *msg, BYTE SendType, std::vector<ull> *pIDVector, WORD wVecLen)
{
    // InterlockedIncrement64(&m_RecvMsgArr[en_PACKET_CS_CHAT_RES_MESSAGE]);
    switch (SendType)
    {
    case 0:
        Unicast(SessionID, msg);
        break;
    case 1:
        BroadCast(SessionID, msg, pIDVector, wVecLen);
        break;
    }
    
}
void CLanClient::Unicast(ull SessionID, CMessage *msg, LONG64 Account)
{
    Profiler profile(L"UnitCast_Cnt");
    if (SessionLock(SessionID) == false)
    {
        stTlsObjectPool<CMessage>::Release(msg);
        return;
    }
    clsSession &session = sessions_vec[SessionID >> 47];

    // 여기까지 왔다면, 같은 Session으로 판단하자.
    CMessage **ppMsg;
    ull local_IoCount;
    ppMsg = &msg;

    {
        Profiler profile(L"LFQ_Push");
        session.m_sendBuffer.Push(msg);

    }

    // PQCS를 시도.
    if (InterlockedCompareExchange(&session.m_flag, 1, 0) == 0)
    {
        ZeroMemory(&session.m_sendOverlapped, sizeof(OVERLAPPED));

        local_IoCount = InterlockedIncrement(&session.m_ioCount);

        PostQueuedCompletionStatus(m_hIOCP, 0, (ULONG_PTR)&session, &session.m_sendOverlapped);
    }

    SessionUnLock(SessionID);
}
void CLanClient::BroadCast(ull SessionID, CMessage *msg, std::vector<ull> *pIDVector, WORD wVecLen)
{
    InterlockedExchange64(&msg->iUseCnt, wVecLen);
    for (WORD i = 0; i < wVecLen; i++)
    {
        ull currentSessionID = (*pIDVector)[i];
        if (SessionLock(currentSessionID) == false)
        {
            stTlsObjectPool<CMessage>::Release(msg);
            continue;
        }
        clsSession &session = sessions_vec[currentSessionID >> 47];

        // 여기까지 왔다면, 같은 Session으로 판단하자.
        CMessage **ppMsg;
        ull local_IoCount;
        ppMsg = &msg;

        {
            Profiler profile(L"LFQ_Push");
            session.m_sendBuffer.push(*ppMsg);
        }

        // PQCS를 시도.
        if (InterlockedCompareExchange(&session.m_flag, 1, 0) == 0)
        {
            ZeroMemory(&session.m_sendOverlapped, sizeof(OVERLAPPED));

            local_IoCount = InterlockedIncrement(&session.m_ioCount);

            PostQueuedCompletionStatus(m_hIOCP, 0, (ULONG_PTR)&session, &session.m_sendOverlapped);
        }

        SessionUnLock(currentSessionID);
    }
}


void CLanClient::RecvComplete(clsSession &session, DWORD transferred)
{
    stHeader header;
    ringBufferSize useSize;

    ull SessionID;
    bool bChkSum = false;
    {
        session.m_recvBuffer.MoveRear(transferred);
        SessionID = session.m_SeqID.SeqNumberAndIdx;
    }
    // Header의 크기만큼을 확인.
    while (session.m_recvBuffer.Peek(&header, headerSize) == headerSize)
    {
        useSize = session.m_recvBuffer.GetUseSize();
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
        {
            {
                Profiler profile(L"DeCoding");
                bChkSum = msg->DeCoding();
            }
            if (bChkSum == false)
            {
                // Attack : 조작된 패킷으로 checkSum이 다름.
                InterlockedExchange(&session.m_blive, 0);
                CancelIoEx((HANDLE)session.m_sock, &session.m_sendOverlapped);
                static bool bOn = false;
                if (bOn == false)
                {
                    bOn = true;
                    CSystemLog::GetInstance()->Log(L"Attack", en_LOG_LEVEL::ERROR_Mode,
                                                   L"%-20s ",
                                                   L" false Packet CheckSum Not Equle ");
                }
                stTlsObjectPool<CMessage>::Release(msg);
                return;
            }
        }

        InterlockedExchange(&msg->ownerID, SessionID);

        msg->_frontPtr = msg->_frontPtr + headerSize;

        // PayLoad를 읽고 무엇인가 처리하는 Logic이 NetWork에 들어가선 안된다.
        {
            Profiler profile(L"OnRecv");
            OnRecv(SessionID, msg);
        }
    }
    RecvPacket(session);
}

void CLanClient::SendComplete(clsSession &session, DWORD transferred)
{
}

void CLanClient::ReleaseComplete(ull SessionID)
{
    // 로직상  Session당 한번만 호출되게 짰음.
    int retval;
    clsSession &session = sessions_vec[SessionID >> 47];
    if (SessionID != session._sessionID)
    {

        __debugbreak();
        return;
    }

    OnRelease(SessionID);
    session.Release();
    retval = closesocket(session._sock);
    DWORD LastError = GetLastError();
    if (retval != 0)
    {

     
    }
    else
    {
        _IdxStack.push(SessionID >> 47);
        //_interlockeddecrement64(&m_SessionCount);
    }
}

void CLanClient::ReleaseSession(ull SessionID)
{
    
    clsSession& session = sessions_vec[SessionID >> 47];
    ZeroMemory(&session.m_releaseOverlapped, sizeof(OVERLAPPED));

    PostQueuedCompletionStatus(m_hIOCP, 0, SessionID, &session.m_releaseOverlapped);

}

bool CLanClient::SessionLock(ull SessionID)
{
    /*
    ※ 인자로 Msg들고오기 싫음.
    => False를 리턴받는다면 호출부에서 msg폐기를 요구합니다.

    * Release된 Session이라면  False를 리턴
    * SessionID가 다르다면     False를 리턴  IO를 감소 시킨후 Release시도 ,  적어도 진입순간에는 Session은 살아있었음.
    * 증가시킨 IO가 '1'인 경우 감소 시킨 후  Release시도 , False를 리턴
    *
*/
    clsSession &session = sessions_vec[SessionID >> 47];
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
    seqID = session._sessionID;
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

void CLanClient::SessionUnLock(ull SessionID)
{
    clsSession &session = sessions_vec[SessionID >> 47];
    ull Local_ioCount;

    Local_ioCount = InterlockedDecrement(&session.m_ioCount);
    // 앞으로 Session 초기화는 IoCount를 '0'으로 하면 안된다.
    // TODO :  ContentsThread에서 Contents_Enq하는 경우의 수. 문제가없는가?
    if (InterlockedCompareExchange(&session.m_ioCount, (ull)1 << 47, 0) == 0)
        ReleaseSession(SessionID);
}

void clsSession::Release()
{
    CMessage *msg;
    while (m_sendBuffer.empty() == false)
    {
        msg = m_sendBuffer.front();
        m_sendBuffer.pop();

        //stTlsObjectPool<CMessage>::Release(msg);
    }

    {
        ZeroMemory(&m_recvOverlapped, sizeof(OVERLAPPED));
        ZeroMemory(&m_sendOverlapped, sizeof(OVERLAPPED));
    }
    m_recvBuffer.ClearBuffer();
}
