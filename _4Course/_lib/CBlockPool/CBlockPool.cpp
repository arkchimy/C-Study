#include "CBlockPool.h"

// TODO: 라이브러리 함수의 예제입니다.
void fnCBlockPool()
{
    CBlockPool pool;
    #define BIG_SIZE 100
    pool.FixedBlockSize(BIG_SIZE); //  이 Pool에서 할당 받아 사용할 객체 중 가장 큰 사이즈로,
    //FixedBlockSize
    {
        //pool.m_Blocksize = BIG_SIZE;
    }
}
