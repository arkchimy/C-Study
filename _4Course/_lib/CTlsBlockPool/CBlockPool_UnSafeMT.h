#pragma once

#include <stack>

using FlowChkType = int64_t;
using NodeCntType = int; // m_Size 체크
static constexpr FlowChkType Alloc_Data = 0xFDFDFDFDFDFDFDFD;
static constexpr FlowChkType Relase_Data = 0xDDDDDDDDDDDDDDDD;

class CBlockPool_UnSafeMT
{
    // 얘는 모든 타입을 받는 stNode여야함.
    struct Block
    {
        // [ Under flow ][ Block ][ payload(m_size) ][ Over flow ]
        Block() noexcept
        {
            _begin = (unsigned char *)this - sizeof(FlowChkType); // 실제 메모리 할당 주소
            _node = _begin + sizeof(FlowChkType) + sizeof(Block);
            next = nullptr;
            _underPtr = (FlowChkType *)(_begin);
            // overPtr  끝부분은 어쩔수없이 여기서 초기화. 생성자에서 금지.
        }
        void *operator new(size_t, size_t objSize)
        {
            unsigned char *_begin;
            unsigned char *_blockPtr;
            unsigned char *overPtr;
            Block *blockPtr;

            _begin = (unsigned char *)malloc(sizeof(Block) + objSize + sizeof(FlowChkType) + sizeof(FlowChkType));
            if (_begin == nullptr)
                __debugbreak(); // 메모리 부족

            _blockPtr = (unsigned char *)_begin + sizeof(FlowChkType);

            *(FlowChkType *)_begin = Relase_Data; // underflowChk
            overPtr = _begin + sizeof(Block) + objSize + sizeof(FlowChkType);
            *(FlowChkType *)overPtr = Relase_Data; // OverflowChk
            blockPtr = reinterpret_cast<Block *>(_begin + sizeof(FlowChkType));
            blockPtr->_overPtr = (FlowChkType *)overPtr; //  끝부분은 어쩔수없이 여기서 초기화. 생성자에서 금지.

            return _begin + sizeof(FlowChkType); // Block의 주소를 넘겨주려고 함.
        }
        void operator delete(void *p) noexcept
        {
            Block *pBlock = (Block *)p;

#ifdef BlockDebugMode
            {
                if (*pBlock->_underPtr != Relase_Data ||
                    *pBlock->_overPtr != Relase_Data)
                {
                    __debugbreak();
                }
            }
#endif

            free(pBlock->_begin);
        }
        void operator delete(void *p, char *File) noexcept
        {
        }
        void operator delete[](void *p, char *File) noexcept
        {
        }
        void *_node; // 실제 객체의 생성자를 호출할 주소.
        Block *next; // 다음에 할당될 Block의 주소.

        FlowChkType *_underPtr; // 언더플로우를 체크할 주소
        unsigned char *_begin;  // Block을 Delete를 해야할 주소.
        FlowChkType *_overPtr;  //  생성자에서 초기화 금지.
    };

  public:
    inline void FixedBlockSize(size_t size) noexcept
    {
        m_Blocksize = size;
    }
    inline size_t GetCreateCnt() const noexcept { return m_newCnt; }
    template <typename T>
    T *Alloc()
    {
        Block *node;
        node = _top;
        _top = node->next;
        m_Size--;
        if (m_Size < 0)
            __debugbreak();

#ifdef BlockDebugMode
        {
            if (*node->_underPtr != Relase_Data ||
                *node->_overPtr != Relase_Data)
            {
                __debugbreak();
            }
            *node->_underPtr = Alloc_Data;
            *node->_overPtr = Alloc_Data;
        }
#endif
        return new (node->_node) T();
    }
    template <typename T>
    void Release(void *ptr)
    {
        Block *node;
        T *tPtr = reinterpret_cast<T *>(ptr);
        tPtr->~T();
        node = reinterpret_cast<Block *>((char *)tPtr - sizeof(Block));

#ifdef BlockDebugMode
        {
            if (*node->_overPtr != Alloc_Data ||
                *node->_underPtr != Alloc_Data)
            {
                __debugbreak();
            }
            *node->_overPtr = Relase_Data;
            *node->_underPtr = Relase_Data;
        }
#endif
        node->next = _top;
        _top = node;
        m_Size++;
    }
    void Initalize(int capacity, int BlockSize) 
    {
        Block *newBlock;

        for (int i = 0; i < capacity; i++)
        {
            newBlock = new (BlockSize) Block();
            newBlock->next = _top;
            _top = newBlock;
        }
        m_Size = capacity;
    }

  private:
    size_t m_Blocksize = 0;
    size_t m_newCnt = 0;
    Block *_top;

  public:
    NodeCntType m_Size = 0;
};
