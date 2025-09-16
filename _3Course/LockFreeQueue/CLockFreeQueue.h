#pragma once
#include "../lib/CLockFreeMemoryPool_Backup/CLockFreeMemoryPool.h"

template <typename T>
class CLockFreeQueue
{
  public:
    CLockFreeQueue();
    ~CLockFreeQueue();
    struct stNode
    {
        T data;
        stNode *next;
    };
    void Push(__in T data);
    T Pop();

    stNode *_head;
    stNode *_tail;

    volatile LONG64 seqNumber;

    CObjectPool<T> pool;
};

template <typename T>
CLockFreeQueue<T>::CLockFreeQueue()
{
    stNode *Dummy = reinterpret_cast<stNode *>(pool.Alloc());
    Dummy->next = nullptr;

    _head = Dummy;
    _tail = _head;
}

template <typename T>
inline CLockFreeQueue<T>::~CLockFreeQueue()
{
    void *ptr = reinterpret_cast<void*>(LONG64(_head) & ADDR_MASK);
    pool.Release(ptr);
}

template <typename T>
void CLockFreeQueue<T>::Push(T data)
{
    LONG64 seq;
    stNode *newNode;

    newNode = reinterpret_cast<stNode *>(pool.Alloc());
    newNode->next = nullptr;

    newNode->data = data;
    seq = _interlockedincrement64(&seqNumber);

    newNode = reinterpret_cast<stNode *>(seq << 47 | (LONG64)newNode);

    reinterpret_cast<stNode *>((LONG64)_tail & ADDR_MASK)->next = newNode;
    _tail = newNode;
}

template <typename T>
T CLockFreeQueue<T>::Pop()
{

    stNode *nextNode;
    stNode *headAddr;

    T outData;

    headAddr = reinterpret_cast<stNode *>((LONG64)_head & ADDR_MASK);
    nextNode = headAddr->next;

    if (headAddr == nullptr)
        __debugbreak();

    outData = reinterpret_cast<stNode *>((LONG64)nextNode & ADDR_MASK)->data;

    pool.Release(headAddr);
    _head = nextNode;

    return outData;
}
