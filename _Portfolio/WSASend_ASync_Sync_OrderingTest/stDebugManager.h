#pragma once
using ull = unsigned long long;
using ll = long long;

extern int g_iSendLoopCnt;
extern ll g_overlapped_id;

enum class en_MessageType
{
    BigMessage,
    SmallMessage,
    None,
};
enum class en_IOType
{
    Pending_ReQuest,
    Sync_ReQuest,
    None,
};

class stDebugManager
{
    stDebugManager() = default;

  public:
    struct stRequestInfo
    {
        ll seqNumber;      // �۾��� ����.
        en_IOType IOType; // * I/O��û�� Sync���� ASync ����.
    };

    struct stCompletionInfo
    {
        ll seqNumber;
        ll RequestSeqNumber;
        en_MessageType _mode; // * BigMessage �� SmalliMessage
    };
    static stDebugManager &GetInstance()
    {
        static stDebugManager instance;
        return instance;
    }
    void ReQuestPush(const en_IOType& ioType ,ll overlappedID);
    void CompletionPush(const en_MessageType & messageType,const ll overlappedID);

    void ReSet();
    void CreateLogFile();

    ll m_ReQuestSeqNumber = 0;
    ll m_CompleteSeqNumber = 0;

    stRequestInfo *m_pReQuestInfos = new stRequestInfo[g_iSendLoopCnt * 2];
    stCompletionInfo *m_pCompleteInfos = new stCompletionInfo[g_iSendLoopCnt * 2];
};