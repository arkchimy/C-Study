// Encoding_TestProgram.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <iostream>
#include "../../_3Course/lib/SerializeBuffer_exception/SerializeBuffer_exception.h"

#define MAX_DATA_LEN 1000
void EnCording(CMessage* msg,BYTE K , BYTE RK)
{

    SerializeBufferSize len = msg->_rearPtr - msg->_frontPtr ;
    BYTE total = 0;


    int current = 1;
    BYTE P = 0;
    BYTE E = 0;

    for (int i = 0; i < len; i++)
    {
        total += msg->_frontPtr[i];
    }
    memcpy(msg->_frontPtr, &total, sizeof(total));

    msg->HexLog();

    for (; &msg->_frontPtr[current] != msg->_rearPtr; current++)
    {
        BYTE D1 = (msg->_frontPtr)[current];
        BYTE b = (P + RK + current );
        P = D1 ^ b;
        E = P ^ (E + K + current );
        msg->_frontPtr[current] = E;
    }
    msg->HexLog();

}
void DeCording(CMessage *msg, BYTE K, BYTE RK)
{
    BYTE P1 = 0,P2;
    BYTE E1 = 0,E2;
    BYTE D1 = 0,D2;
    BYTE total = 0;

    SerializeBufferSize len = msg->_rearPtr - msg->_frontPtr ;
    int current = 1;

    // 2기준
    //D2 ^ (P1 + RK + 2) = P2
    //P2 ^ (E1 + K + 2) = E2

    //E2 ^ (E1 + K + 2) = P2
    //P2 ^ (P1 + RK + 2) = D2
    for (; &msg->_frontPtr[current] != msg->_rearPtr; current++)
    {
        E2 = msg->_frontPtr[current];
        P2  = E2 ^ (E1 + K + current);
        E1 = E2;
        D2 = P2 ^ (P1 + RK + current);
        P1 = P2;
        msg->_frontPtr[current] = D2;
    }

    for (int i = 1; i < len; i++)
    {
        total += msg->_frontPtr[i];
    }
    memcpy(msg->_frontPtr, &total, sizeof(total));

    msg->HexLog();


}
using checkSumSpace = BYTE;


int main()
{
    CMessage msg;
    checkSumSpace checksum_space = 0;
    unsigned char data[] = "aaaaaaaaaabbbbbbbbbbcccccccccc1234567890abcdefghijklmn";

    msg.PutData((const char *)&checksum_space, sizeof(checkSumSpace));
    msg.PutData((const char*)data, sizeof(data));

    msg.HexLog();

    BYTE K = 0xA9;
    BYTE RK = 0x31;
    EnCording(&msg, K, RK);
    DeCording(&msg, K, RK);

}

// 프로그램 실행: <Ctrl+F5> 또는 [디버그] > [디버깅하지 않고 시작] 메뉴
// 프로그램 디버그: <F5> 키 또는 [디버그] > [디버깅 시작] 메뉴

// 시작을 위한 팁: 
//   1. [솔루션 탐색기] 창을 사용하여 파일을 추가/관리합니다.
//   2. [팀 탐색기] 창을 사용하여 소스 제어에 연결합니다.
//   3. [출력] 창을 사용하여 빌드 출력 및 기타 메시지를 확인합니다.
//   4. [오류 목록] 창을 사용하여 오류를 봅니다.
//   5. [프로젝트] > [새 항목 추가]로 이동하여 새 코드 파일을 만들거나, [프로젝트] > [기존 항목 추가]로 이동하여 기존 코드 파일을 프로젝트에 추가합니다.
//   6. 나중에 이 프로젝트를 다시 열려면 [파일] > [열기] > [프로젝트]로 이동하고 .sln 파일을 선택합니다.
