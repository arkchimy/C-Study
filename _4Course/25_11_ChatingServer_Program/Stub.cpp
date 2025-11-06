#include "stdafx.h"
#include "Stub.h"

extern template PVOID stTlsObjectPool<CMessage>::Alloc();       // 암시적 인스턴스화 금지
extern template void stTlsObjectPool<CMessage>::Release(PVOID); // 암시적 인스턴스화 금지

bool Stub::PacketProc(ull SessionID, CMessage *msg, stHeader &header)
{
    // TODO : 추후에  Code로 Case문 바꾸기.
    //  메세지를 보낼때,

    char EchoBuffer[100];

    // DeCode된 데이터가 옴.
    switch (header.byType)
    {
        // 에코 Test
    default:
        // 언마샬링
        msg->GetData(EchoBuffer, header.sDataLen);
        stTlsObjectPool<CMessage>::Release(msg);

        EchoProcedure(SessionID, EchoBuffer); //동적 바인딩
    }

    return true;
}
