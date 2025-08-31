#pragma once
#include <Windows.h>
#include <algorithm>
#include <iostream>
#include <vector>



#define BITMASK 0x7FFFFFFFFFFF
#define ALLOC_Node 0xcccccccc
#define RELEASE_Node 0xeeeeeeee

using ull = unsigned long long;
using ll = long long;

struct stPoolInfo
{
    stPoolInfo() {}

    int mode1 = 0; // 보기 편하도록
    int padding = 0; // 보기 편하도록
    ull tag = 0;
    void *addr = nullptr;
    void *nextNode = nullptr; // 내가 누굴 가리키는지

    DWORD threadId = 0;
    int mode2 = 0;
};

typedef struct stSeqAddr
{
    stSeqAddr() : val(0) {}
    bool operator==(const stSeqAddr& other)
    {
        return this->val == other.val;
    }
    union
    {
        ull val = 0;
        struct
        {
            ull addr : 47;
            ull seqNumber1 : 8; // LockFree 자료구조에서 사용하는 SeqNumber
            ull seqNumber2 : 9; // Pool에서 사용하는 SeqNumber
        };
    };
} stSeqAddr;

template <typename T>
struct stNode
{
    stNode()
    {

        this->seqAddr.addr = (ull)this;
        this->seqAddr.seqNumber1 = 0;
        this->seqAddr.seqNumber2 = 0;
    }

    stNode(ull id)
    {

        this->seqAddr.addr = (ull)this;
        this->seqAddr.seqNumber1 = 0;
        this->seqAddr.seqNumber2 = id;
    }
    T data;
    stSeqAddr seqAddr;
    stNode *next = nullptr;
};

extern std::vector<stPoolInfo> pool_infos;

template <typename T>
class CObjectPool
{
  public:
  public:
    CObjectPool()
    {
        _top = &head.seqAddr;

        head.next = &head;
    }
    ~CObjectPool()
    {
        // TODO : 반환되지않은 메모리가 할당 해제되지 못함.
        stNode<T> *StartNode = reinterpret_cast<stNode<T> *>(_top->addr);
        long top_idx = iNodeCnt;
        stNode<T> *pCurrentNode = StartNode;

        while (pCurrentNode != &head)
        {
            stNode<T> *temp = pCurrentNode;

            pCurrentNode = pCurrentNode->next;
            //RT_ASSERT(pCurrentNode != temp);
            delete temp;
            _InterlockedDecrement(&iNodeCnt);
        }
        if (iNodeCnt != 0)
            __debugbreak();
    }
    stNode<T> *Alloc()
    {
        stNode<T> *oldTop = reinterpret_cast<stNode<T> *>(_top->addr);

        stNode<T> *newTop = nullptr;
        stNode<T> *newNode;
        stSeqAddr temp;
        stSeqAddr oldSeqAddr;

        ll id = _InterlockedIncrement64((ll*)&poolidx);
        DWORD iThreadID = GetCurrentThreadId();
        // Stack에서 빼기
        do
        {
            oldSeqAddr = *_top;

            oldTop = reinterpret_cast<stNode<T> *>(oldSeqAddr.addr);

            // 비어있다는 것을 알아야함.
            if (oldTop == &head)
            {
                // 정보 세팅
                newNode = new stNode<T>(id);
                _InterlockedIncrement(&iNodeCnt);
#ifdef POOLTEST
                pool_infos[id].mode1 = ALLOC_Node;
                pool_infos[id].tag = (newNode->seqAddr.val - newNode->seqAddr.addr);
                pool_infos[id].addr = newNode;
                pool_infos[id].nextNode = nullptr;
                pool_infos[id].threadId = iThreadID;
                pool_infos[id].mode2 = ALLOC_Node;
#endif
                return newNode;
            }
            newTop = oldTop->next;

            temp.addr = (ull)newTop;
            temp.seqNumber1 = newTop->seqAddr.seqNumber1;
            temp.seqNumber2 = id;

        } while (_InterlockedCompareExchange64((ll *)&_top->val, temp.val, oldSeqAddr.val) != oldSeqAddr.val);

#ifdef POOLTEST
        pool_infos[id].mode1 = ALLOC_Node;
        pool_infos[id].tag = (oldTop->seqAddr.val - oldTop->seqAddr.addr);
        pool_infos[id].addr = oldTop;
        pool_infos[id].nextNode = newTop;
        pool_infos[id].threadId = iThreadID;
        pool_infos[id].mode2 = ALLOC_Node;
#endif
        return oldTop;
    }
    void Release(void *newNode)
    {
        // TODO : Push 하기
        stNode<T> *oldTop;
        stNode<T> *newTop = (stNode<T>*)newNode;

        stSeqAddr temp;
        stSeqAddr oldSeqAddr;

        ull id = _InterlockedIncrement64((ll *)&poolidx);
        DWORD iThreadID = GetCurrentThreadId();

        do
        {
            oldSeqAddr = *_top;
            oldTop = reinterpret_cast<stNode<T> *>(oldSeqAddr.addr);
            // TODO : 여기서 oldTop == newTop 인경우가 발생.
            // 아마도 중복으로 사용하고있는 경우가 있는듯 하다.
            // Alloc에서 중복된 것을 주었다?
            // => Alloc시에 락프리에서 매번 Top가 Head인지 체크하는 부분이 빠졌었음.
            // => 해결완료
            newTop->next = oldTop;

            temp.addr = (ull)newTop;
            temp.seqNumber1 = newTop->seqAddr.seqNumber1;
            temp.seqNumber2 = id;

        } while (_InterlockedCompareExchange64((ll *)&_top->val, temp.val, oldSeqAddr.val) != oldSeqAddr.val);

#ifdef POOLTEST
        pool_infos[id].mode1 = RELEASE_Node;
        pool_infos[id].addr = newTop;
        pool_infos[id].nextNode = oldTop;
        pool_infos[id].threadId = iThreadID;
        pool_infos[id].mode2 = RELEASE_Node;
#endif
    }

    stSeqAddr* _top;
    stNode<T> head;

    ull poolidx = -1;
    long iNodeCnt = 0;
};