#pragma once

#pragma pack(1)
class stHeader
{
  public:
    BYTE  byCode;
    BYTE  byType; // 채팅서버니까 프로토콜이 1Byte 
    SHORT sDataLen;
    BYTE  byRandKey;
    BYTE  byCheckSum;
};

#pragma pack()
