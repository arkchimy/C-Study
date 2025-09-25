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
const wchar_t board[] =     L"|           요청 순서        |           동기 or 비동기        |        OVERLAPPED_ID       |        완료 통지 순서            |  순서 틀림  | \n";
const wchar_t Lineboard[] = L"|----------------------------+---------------------------------+----------------------------+----------------------------------+-------------+\n";
const wchar_t format[] =    L"| ReQuest SeqNumber : %5lld | ioType : %20s | ReQuest SeqNumber : %5lld  \|  Completion SeqNumber : %5lld  \| <= \n";
const wchar_t format2[] =   L"| ReQuest SeqNumber : %5lld | ioType : %20s | ReQuest SeqNumber : %5lld  \|  Completion SeqNumber : %5lld  \|  \n";

void stDebugManager::CreateLogFile()
{
    // StringCchPrintfW(LogPrintfBuffer, sizeof(LogPrintfBuffer) / sizeof(wchar_t), L"Not equle  seqNumber :%lld  !=  overlappedID : %lld  IOType %s \n", seqNumber, myOverlapped->_id, "IO_Pending");
    wchar_t LogPrintfBuffer[1500] = {
        0,
    };
    FILE *file;
    _wfopen_s(&file, L"OrderingError.txt", L"w,ccs=utf-16LE");
    if (file != nullptr)
    {
        fwrite(board, sizeof(wchar_t), wcslen(board), file);
        fwrite(Lineboard, sizeof(wchar_t), wcslen(Lineboard), file);

        for (int i = 0; i < g_iSendLoopCnt * 2; i++)
        {
            ZeroMemory(LogPrintfBuffer, sizeof(wchar_t) * 1000);

            switch (m_pReQuestInfos[i].IOType)
            {
            case en_IOType::Pending_ReQuest:
                if (m_pCompleteInfos[i].RequestSeqNumber != m_pCompleteInfos[i].seqNumber)
                    StringCchPrintfW(LogPrintfBuffer, sizeof(LogPrintfBuffer) / sizeof(wchar_t),
                                     format,
                                     m_pReQuestInfos[i].seqNumber, L"Pending_ReQuest",
                                     m_pCompleteInfos[i].RequestSeqNumber, m_pCompleteInfos[i].seqNumber);
                else
                    StringCchPrintfW(LogPrintfBuffer, sizeof(LogPrintfBuffer) / sizeof(wchar_t),
                                     format2,
                                     m_pReQuestInfos[i].seqNumber, L"Pending_ReQuest",
                                     m_pCompleteInfos[i].RequestSeqNumber, m_pCompleteInfos[i].seqNumber);
                break;
            case en_IOType::Sync_ReQuest:
                if (m_pCompleteInfos[i].RequestSeqNumber != m_pCompleteInfos[i].seqNumber)
                    StringCchPrintfW(LogPrintfBuffer, sizeof(LogPrintfBuffer) / sizeof(wchar_t),
                                     format,
                                     m_pReQuestInfos[i].seqNumber, L"Sync_ReQuest",
                                     m_pCompleteInfos[i].RequestSeqNumber, m_pCompleteInfos[i].seqNumber);
                else
                    StringCchPrintfW(LogPrintfBuffer, sizeof(LogPrintfBuffer) / sizeof(wchar_t),
                                     format2,
                                     m_pReQuestInfos[i].seqNumber, L"Sync_ReQuest",
                                     m_pCompleteInfos[i].RequestSeqNumber, m_pCompleteInfos[i].seqNumber);
                break;
            }

            fwrite(LogPrintfBuffer, sizeof(wchar_t), wcslen(LogPrintfBuffer), file);
        }
        fclose(file);
    }
}
