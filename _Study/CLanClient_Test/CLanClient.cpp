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
{    

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
        GetQueuedCompletionStatus(_hIOCP, &transferred, &key, &overlapped, INFINITE);
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

            ull seqID = session->m_SeqID.SeqNumberAndIdx;
            server->ReleaseSession(seqID);
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
        _hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, NULL, logicalProcess - reduceThreadCount);
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
    _sessionVec.resize(userCnt);

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
            // CSystemLog::GetInstance()->Log(L"Socket_Error.txt", en_LOG_LEVEL::ERROR_Mode, L"listen_sock Create Socket Error %d", GetLastError());
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
        _sessionVec[i].id = i;
        _sessionVec[i]._sock = _sockVec[i];

        CreateIoCompletionPort((HANDLE)_sessionVec[i]._sock, _hIOCP, (ULONG_PTR)&_sessionVec[i], 0);
    }




    return false;
}

void CLanClient::RecvComplete(clsSession &session, DWORD transferred)
{
}

void CLanClient::SendComplete(clsSession &session, DWORD transferred)
{
}
