#include "Proxy.h"
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
void Proxy::ReqEcho(ull SessionID, const char *buffer)
{
    CMessage *msg = (CMessage *)stTlsObjectPool<CMessage>::Alloc();
    stHeader header;
    header.byCode = 0x89;
    header.

}
