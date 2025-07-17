#include "CLanServer.h"

CLanServer::CLanServer(unsigned short port)
{
    InitializeCriticalSection(&cs_list);
    SOCKADDR_IN addr;

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8000);

    listen_sock = socket(AF_INET, SOCK_STREAM, 0);

    if (listen_sock == INVALID_SOCKET)
        ServerLogFile("socket() %d \n", GetLastError());

    bind(listen_sock, (sockaddr *)&addr, sizeof(addr));
    listen(listen_sock, SOMAXCONN_HINT(65535));
}

void CLanServer::AcceptThread()
{
    SOCKET client;
    SOCKADDR_IN clientAddr;
    int len = sizeof(clientAddr);

    client = accept(listen_sock, (sockaddr *)&clientAddr, &len);

    if (client == INVALID_SOCKET)
    {
        ServerLogFile("accept() %d \n", GetLastError());
    }
    clsSession<CLanServer> *clpSession = new clsSession<CLanServer>(client, clientAddr);
    printf("Connect  IP : %s  ,Port : %d \n", clpSession->_ipstr, ntohs(clientAddr.sin_port));
    OnAccept(clpSession);
}

void CLanServer::Update()
{
    FD_SET read_set, write_set;
    int select_retval = 0;

    EnterCriticalSection(&cs_list);

    std::list<clsSession<CLanServer> *>::iterator iter = sessions.begin();
    std::list<clsSession<CLanServer> *>::iterator startIter = sessions.begin();
    timeval t_t;
    t_t.tv_usec = 100;
    while (iter != sessions.end())
    {
        FD_ZERO(&read_set);
        FD_ZERO(&write_set);
        startIter = iter;
        for (int i = 0; i < 64; i++)
        {
            FD_SET((*iter)->_sock, &read_set);
            if ((*iter)->recvSize > 0)
                FD_SET((*iter)->_sock, &write_set);
            iter++;
            if (iter == sessions.end())
                break;
        }
        select_retval = select(0, &read_set, &write_set, nullptr, &t_t);
        while (select_retval != 0)
        {
            if (FD_ISSET((*startIter)->_sock, &read_set))
            {
                select_retval--;
                if (OnRecv(*startIter) == false)
                {
                    startIter = sessions.erase(startIter);
                    continue;
                }
            }
            if (select_retval == 0)
                break;
            if (FD_ISSET((*startIter)->_sock, &write_set))
            {
                select_retval--;
                if (OnSend(*startIter) == false)
                {

                    startIter = sessions.erase(startIter);
                    continue;
                }
            }
            startIter++;
        }
    
    }
    LeaveCriticalSection(&cs_list);
}

void CLanServer::OnAccept(clsSession<CLanServer> *session)
{
    EnterCriticalSection(&cs_list);
    sessions.push_back(session);
    LeaveCriticalSection(&cs_list);
}

bool CLanServer::OnRecv(clsSession<CLanServer> *session)
{
    int recvRetval;
    recvRetval = recv(session->_sock, &session->recv_buffer[session->recvSize], 100 - session->recvSize, 0);
    session->recvSize += recvRetval;
    if (recvRetval <= 0)
    {
        printf("DissConnect  IP : %s  ,Port : %d \n", session->_ipstr, ntohs(session->_addr.sin_port));
        return false;
    }
    return true;
}

bool CLanServer::OnSend(clsSession<CLanServer> *session)
{
    int sendSize;
    char buffer[101];

    memcpy(buffer, session->recv_buffer, session->recvSize);
    buffer[session->recvSize] = 0;
    session->recvSize = 0;

    printf("%s\n", buffer);

    sendSize = send(session->_sock, buffer, strlen(buffer), 0);

    if (sendSize <= 0)
    {
        printf("DissConnect  IP : %s  ,Port : %d \n", session->_ipstr, ntohs(session->_addr.sin_port));
        return false;
    }
    return true;
}

void CLanServer::ServerLogFile(const char *str, DWORD lastError)
{

    switch (lastError)
    {
    case WSAEWOULDBLOCK:
        return;
    }
    FILE *logFile;
    char filename[100];
    char buffer[100];

    StringCchPrintfA(filename, 100, "Error__%s.txt", __DATE__);

    fopen_s(&logFile, filename, "a+");
    if (logFile == nullptr)
    {
        printf("fOpenFail : GetLastError() %d\n", GetLastError());
        __debugbreak();
    }

    StringCchPrintfA(buffer, 100, str, lastError);
    fwrite(buffer, 1, strlen(buffer), logFile);

    fclose(logFile);
}
