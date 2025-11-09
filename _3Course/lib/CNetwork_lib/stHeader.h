#pragma once

#pragma pack(1)
class stHeader
{
  public:
    unsigned char byCode;
    unsigned char byType; // 
    short sDataLen;
    unsigned char byRandKey;
    unsigned char byCheckSum;
};

#pragma pack()
