#include "stDebugManager.h"
#include <Windows.h>
#include <iostream>
#include <strsafe.h>

void stDebugManager::ReQuestPush(const en_IOType &ioType, ll overlappedID)
{
    m_pReQuestInfos[m_ReQuestSeqNumber].seqNumber = overlappedID;
    m_pReQuestInfos[m_ReQuestSeqNumber].IOType = ioType; //
    m_ReQuestSeqNumber++;
}

void stDebugManager::CompletionPush(const en_MessageType &messageType, const ll overlappedID)
{
    m_pCompleteInfos[m_CompleteSeqNumber].RequestSeqNumber = overlappedID;
    m_pCompleteInfos[m_CompleteSeqNumber].seqNumber = m_CompleteSeqNumber;
    m_pCompleteInfos[m_CompleteSeqNumber]._mode = messageType;

    m_CompleteSeqNumber++;
}

void stDebugManager::ReSet()
{
    ZeroMemory(m_pReQuestInfos, sizeof(stRequestInfo) * g_iSendLoopCnt);
    ZeroMemory(m_pCompleteInfos, sizeof(stCompletionInfo) * g_iSendLoopCnt);

    m_ReQuestSeqNumber = 0;
    m_CompleteSeqNumber = 0;

    g_overlapped_id = 0;
}
//                        ReQuest SeqNumber : 0      ioType : ASync     |   ReQuest SeqNumber : 1   Completion SeqNumber : 0
const wchar_t board[] =     L"|           요청 순서        |           동기 or 비동기        |        OVERLAPPED_ID       |        완료 통지 순서            | 틀림 |     메세지 종류    |  \n";
const wchar_t Lineboard[] = L"|---------------------------+-------------------------------+----------------------------+--------------------------------+-----+-------------------+\n";
const wchar_t format[] =    L"| ReQuest SeqNumber : %5lld | ioType : %20s | ReQuest SeqNumber : %5lld  \|  Completion SeqNumber : %5lld  \| <=  |   %15s  \|\n";
const wchar_t format2[] =   L"| ReQuest SeqNumber : %5lld | ioType : %20s | ReQuest SeqNumber : %5lld  \|  Completion SeqNumber : %5lld  \|     |  %15s  \|\n";

void stDebugManager::CreateLogFile(bool bError)
{
    // StringCchPrintfW(LogPrintfBuffer, sizeof(LogPrintfBuffer) / sizeof(wchar_t), L"Not equle  seqNumber :%lld  !=  overlappedID : %lld  IOType %s \n", seqNumber, myOverlapped->_id, "IO_Pending");
    wchar_t LogPrintfBuffer[1500] = {
        0,
    };
    SYSTEMTIME stNowTime;
    WCHAR error_filename[MAX_PATH];
    WCHAR normal_filename[MAX_PATH];
    WCHAR *fileName;
    FILE *file;

    GetLocalTime(&stNowTime);

    wsprintf(error_filename, L"error_filename_%d%02d%02d_%02d.%02d.%02d_.txt",
             stNowTime.wYear, stNowTime.wMonth, stNowTime.wDay, stNowTime.wHour, stNowTime.wMinute, stNowTime.wYear);
    wsprintf(normal_filename, L"normal_filename_%d%02d%02d_%02d.%02d.%02d_.txt",
             stNowTime.wYear, stNowTime.wMonth, stNowTime.wDay, stNowTime.wHour, stNowTime.wMinute, stNowTime.wYear);

    if (bError)
    {
        _wfopen_s(&file, error_filename, L"a+,ccs=utf-16LE");
    }
    else
    {
        _wfopen_s(&file, normal_filename, L"w,ccs=utf-16LE");
    }
    
    
    wchar_t bigMessageType[] = L"BigMessage";
    wchar_t smallMessageType[] = L"SmallMessage";
    wchar_t *MessageType = nullptr;

    if (file != nullptr)
    {
        fwrite(board, sizeof(wchar_t), wcslen(board), file);
        fwrite(Lineboard, sizeof(wchar_t), wcslen(Lineboard), file);

        for (int i = 0; i < g_iSendLoopCnt * 2; i++)
        {
            ZeroMemory(LogPrintfBuffer, sizeof(LogPrintfBuffer));

            if (m_pCompleteInfos[i]._mode == en_MessageType::BigMessage)
                MessageType = bigMessageType;
            else
                MessageType = smallMessageType;

            switch (m_pReQuestInfos[i].IOType)
            {

            case en_IOType::Pending_ReQuest:
                if (m_pCompleteInfos[i].RequestSeqNumber != m_pCompleteInfos[i].seqNumber)
                    StringCchPrintfW(LogPrintfBuffer, sizeof(LogPrintfBuffer) / sizeof(wchar_t),
                                     format,
                                     m_pReQuestInfos[i].seqNumber, L"Pending_ReQuest",
                                     m_pCompleteInfos[i].RequestSeqNumber, m_pCompleteInfos[i].seqNumber, MessageType);
                else
                    StringCchPrintfW(LogPrintfBuffer, sizeof(LogPrintfBuffer) / sizeof(wchar_t),
                                     format2,
                                     m_pReQuestInfos[i].seqNumber, L"Pending_ReQuest",
                                     m_pCompleteInfos[i].RequestSeqNumber, m_pCompleteInfos[i].seqNumber, MessageType);
                break;

            case en_IOType::Sync_ReQuest:
                if (m_pCompleteInfos[i].RequestSeqNumber != m_pCompleteInfos[i].seqNumber)
                    StringCchPrintfW(LogPrintfBuffer, sizeof(LogPrintfBuffer) / sizeof(wchar_t),
                                     format,
                                     m_pReQuestInfos[i].seqNumber, L"Sync_ReQuest",
                                     m_pCompleteInfos[i].RequestSeqNumber, m_pCompleteInfos[i].seqNumber, MessageType);
                else
                    StringCchPrintfW(LogPrintfBuffer, sizeof(LogPrintfBuffer) / sizeof(wchar_t),
                                     format2,
                                     m_pReQuestInfos[i].seqNumber, L"Sync_ReQuest",
                                     m_pCompleteInfos[i].RequestSeqNumber, m_pCompleteInfos[i].seqNumber, MessageType);
                break;
            }

            fwrite(LogPrintfBuffer, sizeof(wchar_t), wcslen(LogPrintfBuffer), file);
        }
        fclose(file);
    }
}
