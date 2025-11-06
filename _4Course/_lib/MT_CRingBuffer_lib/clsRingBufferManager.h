#pragma once

class clsRingBufferManager
{
    clsRingBufferManager();

    friend class CRingBuffer;

    char *Alloc(int iSize);

    char *_buffer = nullptr;
    char *virtualMemoryEnd = nullptr;

  public:
    static clsRingBufferManager *GetInstance();
};
