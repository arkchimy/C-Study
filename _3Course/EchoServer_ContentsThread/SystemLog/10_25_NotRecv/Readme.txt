500001 ContentsQ  ->  DisConnect Client 50  
=> Not Recv와 Login Pack을 못받았다고 뜸.
500001 ContentsQ  ->  No DisConnect Client 50  
=> 문제 발생하지 않음.

======== PQCS를 하는 부분에서 PQCS를 하고 하단에 UseFlag를 '0'으로 임의적으로 바꾸어줌. ========

 if (InterlockedCompareExchange(&session->m_flag, 1, 0) == 0)
 {
     ZeroMemory(&session->m_sendOverlapped, sizeof(OVERLAPPED));
     local_IoCount = InterlockedIncrement(&session->m_ioCount);
     if ((local_IoCount & (ull)1 << 47) == 0 && local_IoCount != 1)
     {
         InterlockedExchange(&session->m_Useflag, 0);							//추가 된 부분.
         PostQueuedCompletionStatus(m_hIOCP, 0, (ULONG_PTR)session, &session->m_sendOverlapped);
     }
     else
         local_IoCount = InterlockedDecrement(&session->m_ioCount);
 }
 if (InterlockedExchange(&session->m_Useflag, 0) == 2)							// 만일 IO_Count가 0이 한번이라도 된 순간에. PQCS해놓고 release하는 순간이 생김.
 {
     CSystemLog::GetInstance()->Log(L"Socket", en_LOG_LEVEL::SYSTEM_Mode,
                                    L"%-10s %10s %05lld  %10s %012llu  %10s %4llu  %10s %3llu",
                                    L"ContentsRelease3",
                                    L"HANDLE : ", session->m_sock, L"seqID :", session->m_SeqID.SeqNumberAndIdx, L"seqIndx : ", session->m_SeqID.idx,
                                    L"IO_Count", session->m_ioCount);
     ReleaseSession(sessionID);
 }