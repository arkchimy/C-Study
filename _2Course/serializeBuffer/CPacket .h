#pragma once
#include <Windows.h>

#ifndef __PACKET__
#define __PACKET__

class CPacket
{
  public:
    /*---------------------------------------------------------------
    Packet Enum.

    ----------------------------------------------------------------*/
    enum en_PACKET
    {
        eBUFFER_DEFAULT = 1400 // ��Ŷ�� �⺻ ���� ������.
    };

    //////////////////////////////////////////////////////////////////////////
    // ������, �ı���.
    //
    // Return:
    //////////////////////////////////////////////////////////////////////////
    CPacket();
    CPacket(int iBufferSize);

    virtual ~CPacket();

    //////////////////////////////////////////////////////////////////////////
    // ��Ŷ û��.
    //
    // Parameters: ����.
    // Return: ����.
    //////////////////////////////////////////////////////////////////////////
    void Clear(void);

    //////////////////////////////////////////////////////////////////////////
    // ���� ������ ���.
    //
    // Parameters: ����.
    // Return: (int)��Ŷ ���� ������ ���.
    //////////////////////////////////////////////////////////////////////////
    int GetBufferSize(void) { return m_iBufferSize; }
    //////////////////////////////////////////////////////////////////////////
    // ���� ������� ������ ���.
    //
    // Parameters: ����.
    // Return: (int)������� ����Ÿ ������.
    //////////////////////////////////////////////////////////////////////////
    int GetDataSize(void) { return m_iDataSize; }

    //////////////////////////////////////////////////////////////////////////
    // ���� ������ ���.
    //
    // Parameters: ����.
    // Return: (char *)���� ������.
    //////////////////////////////////////////////////////////////////////////
    char *GetBufferPtr(void) { return m_chpBuffer; }

    //////////////////////////////////////////////////////////////////////////
    // ���� Pos �̵�. (�����̵��� �ȵ�)
    // GetBufferPtr �Լ��� �̿��Ͽ� �ܺο��� ������ ���� ������ ������ ��� ���.
    //
    // Parameters: (int) �̵� ������.
    // Return: (int) �̵��� ������.
    //////////////////////////////////////////////////////////////////////////
    int MoveWritePos(int iSize);
    int MoveReadPos(int iSize);

    /* ============================================================================= */
    // ������ �����ε�
    /* ============================================================================= */
    CPacket &operator=(CPacket &clSrcPacket);

    //////////////////////////////////////////////////////////////////////////
    // �ֱ�.	�� ���� Ÿ�Ը��� ��� ����.
    //////////////////////////////////////////////////////////////////////////
    CPacket &operator<<(unsigned char byValue);
    CPacket &operator<<(char chValue);

    CPacket &operator<<(short shValue);
    CPacket &operator<<(unsigned short wValue);

    CPacket &operator<<(int iValue);
    CPacket &operator<<(long lValue);
    CPacket &operator<<(float fValue);

    CPacket &operator<<(__int64 iValue);
    CPacket &operator<<(double dValue);

    //////////////////////////////////////////////////////////////////////////
    // ����.	�� ���� Ÿ�Ը��� ��� ����.
    //////////////////////////////////////////////////////////////////////////
    CPacket &operator>>(BYTE &byValue);
    CPacket &operator>>(char &chValue);

    CPacket &operator>>(short &shValue);
    CPacket &operator>>(WORD &wValue);

    CPacket &operator>>(int &iValue);
    CPacket &operator>>(DWORD &dwValue);
    CPacket &operator>>(float &fValue);

    CPacket &operator>>(__int64 &iValue);
    CPacket &operator>>(double &dValue);

    //////////////////////////////////////////////////////////////////////////
    // ����Ÿ ���.
    //
    // Parameters: (char *)Dest ������. (int)Size.
    // Return: (int)������ ������.
    //////////////////////////////////////////////////////////////////////////
    int GetData(char *chpDest, int iSize);

    //////////////////////////////////////////////////////////////////////////
    // ����Ÿ ����.
    //
    // Parameters: (char *)Src ������. (int)SrcSize.
    // Return: (int)������ ������.
    //////////////////////////////////////////////////////////////////////////
    int PutData(char *chpSrc, int iSrcSize);

  protected:
    int   m_iBufferSize;
    char* m_chpBuffer;
    //------------------------------------------------------------
    // ���� ���ۿ� ������� ������.
    //------------------------------------------------------------
    int   m_iDataSize;
};

#endif
