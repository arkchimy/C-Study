#include "CLanServer.h"
#include "../../error_log.h"


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


BOOL CLanServer::OnServer(const wchar_t *addr, short port)
{
    DWORD bind_retval;

    SOCKADDR_IN serverAddr;
    ZeroMemory(&serverAddr, sizeof(serverAddr));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    InetPtonW(AF_INET, addr, &serverAddr.sin_addr);

    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    bind_retval = bind(listen_sock, (sockaddr *)&serverAddr, sizeof(serverAddr));
    if (bind_retval != 0)
        ERROR_FILE_LOG(L"Socket_Error.txt", L"Bind Failed");

    if (listen_sock == INVALID_SOCKET)
    {

        ERROR_FILE_LOG(L"Socket_Error.txt",
                       L"listen_sock Create Socket Error");
        return false;
    }
    return true;
}

CLanServer::~CLanServer()
{
    closesocket(listen_sock);
}
