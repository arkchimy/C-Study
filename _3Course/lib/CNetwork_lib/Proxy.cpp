#include "Proxy.h"

#include "CNetwork_lib.h"
#include "stHeader.h"

#include "../../../_4Course/_lib/CTlsObjectPool_lib/CTlsObjectPool_lib.h"
#include "../SerializeBuffer_exception/SerializeBuffer_exception.h"



extern template PVOID stTlsObjectPool<CMessage>::Alloc();       // 암시적 인스턴스화 금지
extern template void stTlsObjectPool<CMessage>::Release(PVOID); // 암시적 인스턴스화 금지
//    
//NetWork::Instance._dfPACKET_SC_CREATE_MY_CHARACTER++;
//stHeader header;
//header.byType = byType;
//header.byCode = 0x89;
//CPacket cPacket;
//cPacket.PutData((char *)&header, sizeof(stHeader));
//cPacket << id << byDirection << x << y << byHP;
//*(cPacket.GetBufferPtr() + 1) = cPacket.GetDataSize() - sizeof(stHeader);
//NetWork::SendPacket(clpSection, &cPacket, bBroadCast);


bool Proxy::LoginProcedure(ull SessionID, CMessage *msg, signed long long AccountNo)
{
    stHeader header;
    CLanServer *server;

    header.byType = 0; // 임의 값
    server = reinterpret_cast<CLanServer *>(this);

    msg->PutData((const char *)&header, server->headerSize);
    *msg << AccountNo;
    server->SendPacket(SessionID, msg, UnitCast); // msg

    return false;
}

void Proxy::ReqEcho(ull SessionID, const char *buffer, unsigned long len)
{
    CMessage *msg = (CMessage *)stTlsObjectPool<CMessage>::Alloc();
    stHeader header;

    header.byType = 0;
}
