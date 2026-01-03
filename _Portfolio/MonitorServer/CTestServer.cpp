#include "CTestServer.h"

bool CTestServer::OnAccept(ull SessionID, SOCKADDR_IN &addr)
{
    return false;
}

void CTestServer::OnRecv(ull SessionID, CMessage *msg, bool bBalanceQ)
{

}

void CTestServer::OnRelease(ull SessionID)
{
}
