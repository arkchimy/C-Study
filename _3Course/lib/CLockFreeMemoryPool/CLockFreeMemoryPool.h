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

    int mode1 = 0; // ���� ���ϵ���
    int padding = 0; // ���� ���ϵ���
    ull tag = 0;
    void *addr = nullptr;
    void *nextNode = nullptr; // ���� ���� ����Ű����

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
            ull seqNumber1 : 8; // LockFree �ڷᱸ������ ����ϴ� SeqNumber
            ull seqNumber2 : 9; // Pool���� ����ϴ� SeqNumber
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
        // TODO : ��ȯ�������� �޸𸮰� �Ҵ� �������� ����.
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
        // Stack���� ����
        do
        {
            oldSeqAddr = *_top;

            oldTop = reinterpret_cast<stNode<T> *>(oldSeqAddr.addr);

            // ����ִٴ� ���� �˾ƾ���.
            if (oldTop == &head)
            {
                // ���� ����
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
        // TODO : Push �ϱ�
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
            // TODO : ���⼭ oldTop == newTop �ΰ�찡 �߻�.
            // �Ƹ��� �ߺ����� ����ϰ��ִ� ��찡 �ִµ� �ϴ�.
            // Alloc���� �ߺ��� ���� �־���?
            // => Alloc�ÿ� ���������� �Ź� Top�� Head���� üũ�ϴ� �κ��� ��������.
            // => �ذ�Ϸ�
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