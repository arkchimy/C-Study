#pragma once

#pragma pack(1)
class stHeader
{
  public:
    BYTE byCode;
    BYTE byType; // 
    SHORT sDataLen;
    BYTE byRandKey;
    BYTE byCheckSum;
};

#pragma pack()
