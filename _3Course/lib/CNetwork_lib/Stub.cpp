#include "Stub.h"

#include "../../../_4Course/_lib/CTlsObjectPool_lib/CTlsObjectPool_lib.h"
#include "../SerializeBuffer_exception/SerializeBuffer_exception.h"
#include "../../EchoServer_ContentsThread/CTestServer.h"

extern template PVOID stTlsObjectPool<CMessage>::Alloc();       // 암시적 인스턴스화 금지
extern template void stTlsObjectPool<CMessage>::Release(PVOID); // 암시적 인스턴스화 금지

void Stub::PacketProc(ull SessionID, CMessage *msg, stHeader &header,WORD type)
{
    // TODO : 추후에  Code로 Case문 바꾸기.
    //  메세지를 보낼때,

    char EchoBuffer[1000];
    bool bSucess;
    CTestServer *server = (CTestServer *)this;
    // DeCode된 데이터가 옴.
    switch (type)
    {
    case en_PACKET_CS_CHAT_REQ_LOGIN:
        INT64 AccontNo;
        WCHAR ID[20];
        WCHAR Nickname[20];
        char SessionKey[64];

        *msg >> AccontNo;
        msg->GetData((char*) &ID, sizeof(ID));
        msg->GetData((char *)&Nickname, sizeof(Nickname));
        msg->GetData((char *)&SessionKey, sizeof(SessionKey));
        msg->~CMessage();
      
        server->LoginProcedure(SessionID,msg, AccontNo, ID, Nickname, SessionKey);
        break;
    case en_PACKET_CS_CHAT_REQ_ECHO:
        //
        //    {
        //      WORD   Type => 250
        //      WORD   MessageLen
        //      CHAR   Message[MessageLen]
        //   }
        WORD MessageLen;
        *msg >> MessageLen;
        msg->GetData(EchoBuffer, MessageLen);

        server->EchoProcedure(SessionID, msg, EchoBuffer, MessageLen); // 동적 바인딩
        break;
    default:
        // 에코 Test
        break;
    }
  
}
