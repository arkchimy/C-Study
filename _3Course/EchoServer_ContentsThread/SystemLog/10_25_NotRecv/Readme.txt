500001 ContentsQ  ->  DisConnect Client 50  
=> Not Recv�� Login Pack�� ���޾Ҵٰ� ��.
500001 ContentsQ  ->  No DisConnect Client 50  
=> ���� �߻����� ����.

======== PQCS�� �ϴ� �κп��� PQCS�� �ϰ� �ϴܿ� UseFlag�� '0'���� ���������� �ٲپ���. ========

 if (InterlockedCompareExchange(&session->m_flag, 1, 0) == 0)
 {
     ZeroMemory(&session->m_sendOverlapped, sizeof(OVERLAPPED));
     local_IoCount = InterlockedIncrement(&session->m_ioCount);
     if ((local_IoCount & (ull)1 << 47) == 0 && local_IoCount != 1)
     {
         InterlockedExchange(&session->m_Useflag, 0);							//�߰� �� �κ�.
         PostQueuedCompletionStatus(m_hIOCP, 0, (ULONG_PTR)session, &session->m_sendOverlapped);
     }
     else
         local_IoCount = InterlockedDecrement(&session->m_ioCount);
 }
 if (InterlockedExchange(&session->m_Useflag, 0) == 2)							// ���� IO_Count�� 0�� �ѹ��̶� �� ������. PQCS�س��� release�ϴ� ������ ����.
 {
     CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                    L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                    L"ContentsRelease3",
                                    L"HANDLE : ", session->m_sock, L"seqID :", session->m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session->m_SeqID.idx,
                                    L"IO_Count", session->m_ioCount);
     ReleaseSession(sessionID);
 }