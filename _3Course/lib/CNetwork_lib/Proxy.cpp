#include "Proxy.h"

#include "../../EchoServer_ContentsThread/CTestServer.h"
#include "stHeader.h"

#include "../../../_4Course/_lib/CTlsObjectPool_lib/CTlsObjectPool_lib.h"
#include "../SerializeBuffer_exception/SerializeBuffer_exception.h"

extern template PVOID stTlsObjectPool<CMessage>::Alloc();       // 암시적 인스턴스화 금지
extern template void stTlsObjectPool<CMessage>::Release(PVOID); // 암시적 인스턴스화 금지
//
// NetWork::Instance._dfPACKET_SC_CREATE_MY_CHARACTER++;
// stHeader header;
// header.byType = byType;
// header.byCode = 0x89;
// CPacket cPacket;
// cPacket.PutData((char *)&header, sizeof(stHeader));
// cPacket << id << byDirection << x << y << byHP;
//*(cPacket.GetBufferPtr() + 1) = cPacket.GetDataSize() - sizeof(stHeader);
// NetWork::SendPacket(clpSection, &cPacket, bBroadCast);

bool Proxy::LoginProcedure(ull SessionID, CMessage *msg, INT64 AccountNo)
{
    stHeader header;
    CLanServer *server;

    server = reinterpret_cast<CLanServer *>(this);
    header.byType = 0x89; // 임의 값

    msg->PutData(&header, server->headerSize);
    *msg << AccountNo;

    if (server->bEnCording)
        msg->EnCoding();

    server->SendPacket(SessionID, msg, UnitCast); // msg

    return true;
}


void Proxy::EchoProcedure(ull SessionID, CMessage *msg, char *const buffer, short len)
{
    stHeader header;
    CLanServer *server;

    server = reinterpret_cast<CTestServer *>((char*)this - 8);
    header.byType = en_PACKET_CS_ECHO;
    header.byCode = 0x89;
    header.sDataLen = len;

    msg->~CMessage();

    msg->PutData(&header, server->headerSize);
    msg->PutData(buffer, len);
    
    if (server->bEnCording)
        msg->EnCoding();

    server->SendPacket(SessionID, msg, UnitCast); // msg
}
