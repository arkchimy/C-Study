// CTlsBlockPool.cpp : 정적 라이브러리를 위한 함수를 정의합니다.
//
#include "CTlsBlockPool.h"
// TODO: 라이브러리 함수의 예제입니다.
void fnCTlsBlockPool()
{
    thread_local CTlsBlockPool* pool;

    size_t capacity = 100;  // 한번에 할당할 Node의 수
    size_t BlockSize = 100; // TlsPool을 사용하는 객체중 가장 큰사이즈로.

    CTlsBlockPoolManager *manager = new CTlsBlockPoolManager(capacity , BlockSize);

    pool = new CTlsBlockPool();
    pool->InitTlsPool(manager);  // 해당 Pool 의 Manager를 배치하기.
    int* node = pool->Alloc<int>();
    *node = 3;

    pool->Release<int>(node);
    
}
