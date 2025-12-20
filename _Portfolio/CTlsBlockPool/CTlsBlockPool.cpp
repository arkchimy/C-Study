#include "CTlsBlockPool.h"

#include <Windows.h>

void CTlsBlockPool::InitTlsPool(CTlsBlockPoolManager *pManagersize_t)
{
    m_Capacity = pManagersize_t->m_tlsPool_init_Capacity;
}
