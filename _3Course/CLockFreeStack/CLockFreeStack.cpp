
#include "../lib/CLockFreeMemoryPool_Backup/CLockFreeMemoryPool.h"
#include "../lib/CrushDump_lib/CrushDump_lib/CrushDump_lib.h"
#include <iostream>
#include <thread>

CDump dump;
#define INFO_BUFFER_MAX 100
#define DELETE_MODE 0xDDDDDDDD
#define PUSH_MODE 0xEEEEEEEE

template <typename T>
class CLockFreeStack
{
    struct stNode
    {
        T data;
        stNode *next;
    };
    struct stDebug
    {
        LONG64 seqNumber; // 0 + 8 = 8
        DWORD threadID;   // 8 + (4) + 4 = 16
        DWORD mode;
        stNode *currentTop;          // 16 + 8 = 24
        stNode *currentTop_nextNode; // 24 + 8 = 32

        stNode *newTop;          // 32 + 8 = 40
        stNode *newTop_nextNode; // 40 + 8 = 48
    }; // 48

  public:
    CLockFreeStack()
    {
        dummy = reinterpret_cast<stNode*>(m_pool.Alloc());
        m_top = dummy;
    }
    ~CLockFreeStack()
    {
        stNode *newTop;

        while (m_size > 0)
        {
            newTop = reinterpret_cast<stNode*>((LONG64)m_top & ADDR_MASK)->next;
            m_pool.Release(reinterpret_cast<stNode *>((LONG64)m_top & ADDR_MASK));
            m_top = newTop;

            m_size--;
        }

        if (m_top != dummy)
            __debugbreak();

        m_pool.Release(reinterpret_cast<stNode *>((LONG64)dummy & ADDR_MASK));
    }
    void push(T data)
    {
        stNode *oldTop;
        stNode *newNode = reinterpret_cast<stNode *>(m_pool.Alloc());
        LONG64 local_seqNumber;
        newNode->data = data;
        local_seqNumber = _interlockedincrement64(&m_seqNumber);
        newNode = reinterpret_cast<stNode*>((LONG64)newNode | ((LONG64) local_seqNumber << 47));

        do
        {
            oldTop = m_top;
            reinterpret_cast<stNode *>((LONG64)newNode & ADDR_MASK)->next = oldTop;
        } while (InterlockedCompareExchangePointer((void **)&m_top, newNode, oldTop) != oldTop);

        _interlockedincrement64(&m_size);

        //{
        //     local_seqNumber = _interlockedincrement64(&m_debugSeqNumber);
        //    //TODO:Masking 구현전 Debug 라  수정해야함
        //    infos[local_seqNumber % INFO_BUFFER_MAX].seqNumber = local_seqNumber;
        //    infos[local_seqNumber % INFO_BUFFER_MAX].threadID = GetCurrentThreadId();
        //    infos[local_seqNumber % INFO_BUFFER_MAX].mode = PUSH_MODE;
        //    infos[local_seqNumber % INFO_BUFFER_MAX].currentTop = oldTop;
        //    infos[local_seqNumber % INFO_BUFFER_MAX].currentTop_nextNode = oldTop->next;
        //    infos[local_seqNumber % INFO_BUFFER_MAX].newTop = newNode;
        //    infos[local_seqNumber % INFO_BUFFER_MAX].newTop_nextNode = newNode->next;
        //}
    }
    T pop()
    {
        stNode *newTop;
        stNode *oldTop;
        T outData;
        LONG64 local_seqNumber;
        do
        {
            oldTop = m_top;
            newTop = reinterpret_cast<stNode *>((LONG64)oldTop & ADDR_MASK)->next;
            outData = reinterpret_cast<stNode *>((LONG64)oldTop & ADDR_MASK)->data;

        } while (InterlockedCompareExchangePointer((void **)&m_top, newTop, oldTop) != oldTop);

        _interlockeddecrement64(&m_size);
 
        //{
        //    //     local_seqNumber = _interlockedincrement64(&m_debugSeqNumber);
        //    //    //TODO:Masking 구현전 Debug 라  수정해야함
        //    infos[local_seqNumber % INFO_BUFFER_MAX].seqNumber = local_seqNumber;
        //    infos[local_seqNumber % INFO_BUFFER_MAX].threadID = GetCurrentThreadId();
        //    infos[local_seqNumber % INFO_BUFFER_MAX].mode = DELETE_MODE;
        //    infos[local_seqNumber % INFO_BUFFER_MAX].currentTop = oldTop;
        //    infos[local_seqNumber % INFO_BUFFER_MAX].currentTop_nextNode = oldTop->next;
        //    infos[local_seqNumber % INFO_BUFFER_MAX].newTop = newTop;
        //    infos[local_seqNumber % INFO_BUFFER_MAX].newTop_nextNode = newTop->next;
        //}

        m_pool.Release(reinterpret_cast<stNode *>((LONG64)oldTop & ADDR_MASK));
        return outData;
    }

    LONG64 m_debugSeqNumber = -1;
    LONG64 m_seqNumber = 0;

    stDebug infos[INFO_BUFFER_MAX];
    LONG64 m_size = 0;

    CObjectPool<T> m_pool;
    stNode *m_top;
    stNode *dummy;
};

HANDLE hStartEvent;
bool bOn = true;
unsigned WorkerThread(void *arg)
{
    CLockFreeStack<int> *stack;

    stack = reinterpret_cast<CLockFreeStack<int> *>(arg);

    WaitForSingleObject(hStartEvent, INFINITE);

    for (int i =0; i < 10000; i++)
    {
        stack->push(0);
        stack->pop();
    }

    return 0;
}
#define WORKERTHREAD_CNT 4
int main()
{
    HANDLE hThread[WORKERTHREAD_CNT];

    while (1)
    {
        CLockFreeStack<int> stack;

        hStartEvent = CreateEvent(nullptr, true, false, nullptr);
        for (int i = 0; i < WORKERTHREAD_CNT; i++)
            hThread[i] = (HANDLE)_beginthreadex(nullptr, 0, WorkerThread, &stack, 0, nullptr);

        SetEvent(hStartEvent);
        WaitForMultipleObjects(WORKERTHREAD_CNT, hThread, true, INFINITE);
        ResetEvent(hStartEvent);
        for (int i = 0; i < WORKERTHREAD_CNT; i++)
            CloseHandle(hThread[i]);
    }

}
