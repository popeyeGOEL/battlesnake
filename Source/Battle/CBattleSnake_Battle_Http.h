#pragma once
#include "PlayerData.h"
#include <list>
#include "cMemoryPool.h"

using namespace TUNJI_LIBRARY;
using namespace PLAYER_DATA;
class CBattleSnake_BattlePlayer_NetSession;
class CBattleSnake_Battle_Http : public cHttp_Async
{
public:
	CBattleSnake_Battle_Http();
	virtual ~CBattleSnake_Battle_Http();

	void Start_MessageThread(void);
	void Stop_MessageThread(void);
	bool Send_Select_Account(UINT64 clientID, UINT64 accountNo);
	bool Send_Select_Contents(UINT64 clientID, UINT64 accountNo);
	bool Send_Update_Contents(UINT64 clientID, UINT64 accountNo, stRecord &record);
	void Send_Http_Heartbeat(void);

	virtual void OnLeaveServer(UINT64 requestID);
	virtual void OnRecv(UINT64 requestID, char* dsPacket, int dataLen, int httpCode = 200);

	///-----------------------------------------------------
	///	URL
	///-----------------------------------------------------
	void Set_SelectAccountURL(WCHAR *selectAccountURL, int len);
	//void Set_UpdateAccountURL(WCHAR* updateAccountURL, int len);
	void Set_SelectContentsURL(WCHAR* selectContentsURL, int len);
	void Set_UpdateContentsURL(WCHAR* updateContentsURL, int len);

private:

	///-----------------------------------------------------
	///	Session
	///-----------------------------------------------------
	enum RequestType
	{
		NONE = 0,
		SELECT_ACCOUNT,
		SELECT_CONTENTS,
		UPDATE_CONTENTS,
	};

	struct st_REQUEST_INFO
	{
		UINT64			requestID;	
		UINT64			clientID;
		RequestType		type;
		bool			bRecv;
		int				lastPacket;
	};

	st_REQUEST_INFO* Alloc_RequestInfo(UINT64 clientID);
	void Free_RequestInfo(st_REQUEST_INFO *pInfo);
	st_REQUEST_INFO* Find_RequestInfoByRequestID(UINT64 requestID);

	///-----------------------------------------------------
	///	Packet Proc
	///-----------------------------------------------------
	bool PacketProc(st_REQUEST_INFO* pInfo, char* dsPacket);

	bool Response_Select_Account(st_REQUEST_INFO *pInfo, char *dsPacket);
	bool Response_Select_Contents(st_REQUEST_INFO *pInfo, char *dsPacket);
	bool Response_Update_Contents(st_REQUEST_INFO *pInfo, char *dsPacket);

	///-----------------------------------------------------
	///	Json Encode / Decode
	///-----------------------------------------------------
	void JsonEncode_AccountNo(StringBuffer &buffer, UINT64 accountNo);
	void JsonEncode_Contents(StringBuffer &buffer, UINT64 accountNo, stRecord &record);

	void JsonDecode_Select_Account(char* pJson, int &result, WCHAR* nickname, char* sessionKey);
	void JsonDecode_Select_Contents(char* pJson, int &result, stRecord &record);
	void JsonDecode_Update_Contents(char* pJson, int& result);

	///-----------------------------------------------------
	///	Request Message Thread
	///-----------------------------------------------------
	static unsigned __stdcall MessageThread(void * arg);
	unsigned int Update_MessageThread(void);
	void Check_HeartBeat(void);

	enum en_MESSAGE_TYPE
	{
		en_MSG_TYPE_SEND = 0,
		en_MSG_TYPE_RECV,
		en_MSG_TYPE_LEAVE,
		en_MSG_TYPE_HEARTBEAT,
		en_MSG_TYPE_TERMINATE,
	};

	struct st_MESSAGE
	{
		int			type;
		UINT64		requestID;
		UINT64		clientID;
		RequestType	reqType;
		int			httpCode;
		WCHAR		json_req[1024];
		char		json_res[1024];
		int			reqLen;
		DWORD		requestTime;
	};

	HANDLE								_hMessageThread;
	CONDITION_VARIABLE					_cnMsgQueueIsNotEmpty;
	cLockFreeQueue<st_MESSAGE*>			_msgQueue;
	SRWLOCK								_msgLock;
	cMemoryPool<st_MESSAGE, st_MESSAGE>		_msgPool;

private:
	///-----------------------------------------------------                   
	///	Variables
	///-----------------------------------------------------
	cMemoryPool<st_REQUEST_INFO, st_REQUEST_INFO>	_requestInfoPool;
	std::list<st_REQUEST_INFO*>			_requestInfoList;

	WCHAR								_selectAccountURL[128];
	int									_selectAccountURL_Length;
	WCHAR								_selectContentsURL[128];
	int									_selectContentsURL_Length;
	WCHAR								_updateContentsURL[128];
	int									_updateContentsURL_Length;

public:
	UINT64								_monitor_msgPool;
	UINT64								_monitor_msgQueue;
	UINT64								_monitor_Info;
	UINT64								_monitor_InfoPool;
	UINT64								_monitor_requestFail;
};

extern CBattleSnake_Battle_Http * g_pHttp;