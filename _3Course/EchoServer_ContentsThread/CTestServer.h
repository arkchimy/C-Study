#pragma once
#include "../lib/CNetwork_lib/CNetwork_lib.h"
#include "../lib/MT_CRingBuffer_lib/MT_CRingBuffer_lib.h"
#include    "../lib/SerializeBuffer_exception/SerializeBuffer_exception.h"

class CTestServer : public CLanServer
{
  public:
    CTestServer();
    virtual ~CTestServer();
    virtual double OnRecv(ull SessionID, CMessage *msg) override;

    CRITICAL_SECTION cs_ContentQ;

    CRingBuffer m_ContentsQ = CRingBuffer(100000);
};