#pragma once

#define WIN32_LEAN_AND_MEAN // 거의 사용되지 않는 내용을 Windows 헤더에서 제외합니다.
#include "../CTlsObjectPool/CTlsObjectPool.h"


template <typename T>
class CTlsLockFreeQueue
{
  public:
    CTlsLockFreeQueue();
    ~CTlsLockFreeQueue();
    struct stNode
    {
        T data;
        stNode *next;
    };
    void Push(__in T data);
    bool Pop(__out T &outData);

    stNode *_head;
    stNode *_tail;

    volatile LONG64 seqNumber = 0;


    LONG64 m_size;
    stNode* m_NullptrNode = nullptr;
    inline static LONG64 s_NullptrSeqNumber  = 0 ;
};

template <typename T>
CTlsLockFreeQueue<T>::CTlsLockFreeQueue()
{
    stNode *Dummy = nullptr;

    m_NullptrNode = reinterpret_cast<stNode *>(stTlsObjectPool<T>::Alloc());
    Dummy = reinterpret_cast<stNode *>(stTlsObjectPool<T>::Alloc());
    if (m_NullptrNode == nullptr)
        __debugbreak();
    if (Dummy == nullptr)
        __debugbreak();
    m_NullptrNode->next = nullptr;
    Dummy->next = m_NullptrNode;

    _head = Dummy;
    _tail = _head;
    m_size = 0;
}

template <typename T>
inline CTlsLockFreeQueue<T>::~CTlsLockFreeQueue()
{
    stNode *ptr = reinterpret_cast<stNode *>(LONG64(_head) & ADDR_MASK);
    T data;
    while (m_size != 0)
    {
        Pop(data);
    }
    ptr = reinterpret_cast<stNode *>(LONG64(_head) & ADDR_MASK);

    if (m_NullptrNode != ptr->next)
    {
        CSystemLog::GetInstance()->Log(L"TlsQueue_Error", en_LOG_LEVEL::ERROR_Mode, L"10%s %p : %p ptr : %p",
                                       L"m_NullptrNode != ptr->next", m_NullptrNode, ptr->next, ptr);
        __debugbreak();
    }

    stTlsObjectPool<T>::Release(ptr);
}

template <typename T>
void CTlsLockFreeQueue<T>::Push(T data)
{
    LONG64 seq;
    stNode *newNode;

    stNode *tail;
    stNode *tailAddr;

    stNode *tailNext;
    static ull PushCnt = 0;
    
    CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode,
                                   L"%-10s %05lld  ",
                                   L"CTlsLFQ_PushCnt", InterlockedIncrement(&PushCnt));

    newNode = reinterpret_cast<stNode *>(stTlsObjectPool<T>::Alloc());
    newNode->next = m_NullptrNode;
    newNode->data = data;

    seq = _interlockedincrement64(&seqNumber);
    _interlockedincrement64(&m_size);
    newNode = reinterpret_cast<stNode *>(seq << 47 | (LONG64)newNode);

    do
    {
        tail = _tail;
        tailAddr = reinterpret_cast<stNode *>((LONG64)tail & ADDR_MASK);
        tailNext = tailAddr->next;

        if (tailNext != m_NullptrNode)
        {
            InterlockedCompareExchangePointer((PVOID *)&_tail, tailNext, tail);
            continue;
        }
        if (InterlockedCompareExchangePointer((PVOID *)&(tailAddr->next), newNode, m_NullptrNode) == m_NullptrNode)
            break;

    } while (1);

    if (InterlockedCompareExchangePointer((PVOID *)&_tail, newNode, tail) != tail)
    {
        InterlockedCompareExchangePointer((PVOID *)&_tail, newNode, tail);
    }
    if (m_NullptrNode->next != nullptr)
        __debugbreak();

}

template <typename T>
bool CTlsLockFreeQueue<T>::Pop(__out T &outData)
{

    stNode *nextNode;
    stNode *headAddr;
    stNode *head;

    stNode *tail;

    stNode *tailNextNode;

    LONG64 local_Size;

    local_Size = _InterlockedDecrement64(&m_size);
    if (local_Size < 0)
    {
        _interlockedincrement64(&m_size);
        outData = T();
        return false;
    }
    static ull PopCnt = 0;

    CSystemLog::GetInstance()->Log(L"TlsObjectPool", en_LOG_LEVEL::DEBUG_Mode,
                                   L"%-10s %05lld  ",
                                   L"CTlsLFQ_PopCnt", InterlockedIncrement(&PopCnt));

    do
    {
        head = _head;
        tail = _tail;

        if (head == tail)
        {
            tailNextNode = reinterpret_cast<stNode *>((ull)tail & ADDR_MASK)->next;
            if (tailNextNode == m_NullptrNode)
                continue;
            InterlockedCompareExchangePointer((PVOID *)&_tail, tailNextNode, tail);
        }

        headAddr = reinterpret_cast<stNode *>((LONG64)head & ADDR_MASK);
        nextNode = headAddr->next;
        if (nextNode == m_NullptrNode)
            continue;
        outData = reinterpret_cast<stNode *>((LONG64)nextNode & ADDR_MASK)->data;
        if (InterlockedCompareExchangePointer((PVOID *)&_head, nextNode, head) == head)
            break;

    } while (1);

    if (m_NullptrNode->next != nullptr)
        __debugbreak();
    stTlsObjectPool<T>::Release(headAddr);

    return true;
}
