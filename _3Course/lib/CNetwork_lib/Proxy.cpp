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

void Proxy::LoginProcedure(ull SessionID, CMessage *msg, INT64 AccountNo)
{
    stHeader header;
    CLanServer *server;

    server = reinterpret_cast<CTestServer *>((char *)this - 8);
    header.byCode = 0x89; // 임의 값

    msg->PutData(&header, server->headerSize);
    *msg << AccountNo;

    if (server->bEnCording)
        msg->EnCoding();

    server->SendPacket(SessionID, msg, UnitCast); // msg
}


void Proxy::EchoProcedure(ull SessionID, CMessage *msg, char * buffer, short len)
{
    //struct stHeader
    //{
    //  public:
    //    BYTE byCode;
    //    SHORT sDataLen;
    //    BYTE byRandKey;
    //    BYTE byCheckSum;
    //};
    // 
    //    {
    //      WORD   Type => 250
    //      WORD   MessageLen
    //      CHAR   Message[MessageLen]
    //   }
    stHeader header;
    CLanServer *server;
    WORD type = en_PACKET_CS_CHAT_REQ_ECHO;

    server = reinterpret_cast<CTestServer *>((char*)this - 8);
    header.byCode = 0x89;

    msg->~CMessage();
 
    msg->PutData(&header, server->headerSize);
    *msg << type;
    *msg << len << buffer;

    msg->PutData(buffer, len);
    
    short *pheaderlen = (short*)(msg->_frontPtr + offsetof(stHeader, sDataLen));
    *pheaderlen = msg->_rearPtr - msg->_frontPtr - server->headerSize;
    if (server->bEnCording)
    {
        msg->EnCoding();
    }

    server->SendPacket(SessionID, msg, UnitCast); // msg
}
