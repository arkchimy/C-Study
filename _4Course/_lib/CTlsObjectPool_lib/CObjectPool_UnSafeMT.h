#pragma once

#define SEQ_MASK 0XFFFF800000000000
#define ADDR_MASK 0x00007FFFFFFFFFFF

#define ALLOC_Node 0xcccccccc
#define RELEASE_Node 0xeeeeeeee

using ull = unsigned long long;
using ll = long long;

extern std::vector<stPoolInfo> pool_infos;

template <typename T>
class CObjectPool_UnSafeMT
{
    struct stPoolInfo
    {
        stPoolInfo() {}

        int mode1 = 0;   // 보기 편하도록
        int padding = 0; // 보기 편하도록
        ull tag = 0;
        void *addr = nullptr;
        void *nextNode = nullptr; // 내가 누굴 가리키는지

        DWORD threadId = 0;
        int mode2 = 0;
    };
    struct stNode
    {
        T data;
        stNode *next;
    };

  public:
    CObjectPool_UnSafeMT()
    {
        m_Top = &m_Dummy; // Dummy m_Dummy
        m_Dummy.next = &m_Dummy;
    }
    ~CObjectPool_UnSafeMT()
    {
        // TODO : 반환되지않은 메모리가 할당 해제되지 못함.
        stNode *StartNode = reinterpret_cast<stNode *>(m_Top);
        LONG64 top_idx = m_size;
        stNode *pCurrentNode = StartNode;

        while (pCurrentNode != &m_Dummy)
        {
            stNode *temp = pCurrentNode;

            pCurrentNode = pCurrentNode->next;
            delete temp;
            m_size--;
        }
        if (m_size != 0)
            __debugbreak();
    }
    void Initalize(DWORD iSize)
    {
        for (DWORD i = 0; i < iSize; i++)
            Release(new stNode());
    }
    PVOID Alloc()
    {
        stNode *oldTop;
        stNode *newTop;
        stNode *ret_Node;

        // Stack에서 빼기

        oldTop = reinterpret_cast<stNode *>(m_Top);

        // 비어있다는 것을 알아야함.
        if (oldTop == &m_Dummy)
        {
            // 정보 세팅
            oldTop = new stNode();
#ifdef POOLTEST

#endif
            return oldTop;
        }
        ret_Node = oldTop;
        newTop = ret_Node->next;

        m_Top = newTop;

        m_size--;
        return ret_Node;
    }
    void Release(PVOID newNode)
    {
        // TODO : Push 하기
        stNode *oldTop;

        oldTop = reinterpret_cast<stNode *>(m_Top);

        reinterpret_cast<stNode *>(newNode)->next = oldTop;

        m_Top = newNode;
        m_size++;
    }

    PVOID m_Top;
    stNode m_Dummy;

    ll seqNumber = -1;
    ll m_size = 0;
};