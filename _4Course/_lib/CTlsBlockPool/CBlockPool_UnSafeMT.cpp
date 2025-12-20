#include "CBlockPool_UnSafeMT.h"

void CBlockPool_UnSafeMT::Initalize(size_t capacity, size_t BlockSize)
{
    Block *newBlock;

    for (size_t i = 0; i < capacity; i++)
    {
        newBlock = new (BlockSize) Block();
        newBlock->next = _top;
        _top = newBlock;
    }
}
