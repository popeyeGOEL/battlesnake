#pragma once
using namespace TUNJI_LIBRARY;

#include <list>
#include "cMemoryPool.h"

class CBattleSnake_Match_NetServer;
class CBattleSnake_Match_Http : public cHttp_Async
{
public:
	CBattleSnake_Match_Http(CBattleSnake_Match_NetServer *pNetServer);
	virtual ~CBattleSnake_Match_Http();

	void Start_MessageThread(void);
	void Stop_MessageThread(void);
	bool Send_Select_Account(UINT64 clientKey, UINT64 accountNo);
	void Send_Http_Heartbeat(void);

	virtual void OnLeaveServer(UINT64 requestID);
	virtual void OnRecv(UINT64 requestID, char *dsPacket, int dataLen, int httpCode = 200);

	///-----------------------------------------------------
	///	URL
	///-----------------------------------------------------
	void Set_SelectAccountURL(WCHAR* selectAccountURL, int len);

private:
	///-----------------------------------------------------
	///	Session
	///-----------------------------------------------------
	enum RequestType
	{
		NONE = 0,
		SELECT_ACCOUNT = 1,
	};

	struct st_REQUEST_INFO
	{
		UINT64		requestID;
		UINT64		clientKey;
		RequestType	type;
		bool		bRecv;
		int			lastPacket;
	};

	st_REQUEST_INFO* Alloc_RequestInfo(UINT64 clientKey);
	void Free_RequestInfo(st_REQUEST_INFO *pInfo);
	st_REQUEST_INFO* Find_RequestInfoByRequestID(UINT64 requestID);

	///-----------------------------------------------------
	///	Packet Proc
	///-----------------------------------------------------
	bool PacketProc(st_REQUEST_INFO *pInfo, char *dsPacket);
	bool Response_Select_Account(st_REQUEST_INFO *pInfo, char *dsPacket);

	///-----------------------------------------------------
	///	Json Encode/Deocde
	///-----------------------------------------------------
	void JsonEncode_Select_Account(StringBuffer &buffer, UINT64 accountNo);
	void JsonDecode_Select_Account(char *pJson, int& result, char *sessionKey);

	///-----------------------------------------------------
	///	Request Message Thread
	///-----------------------------------------------------
	static unsigned __stdcall MessageThread(void *arg);
	unsigned int Update_MessageThread(void);
	void Check_HeartBeat(void);

	enum en_MESSAGE_TYPE
	{
		en_MSG_TYPE_SEND = 0,
		en_MSG_TYPE_RECV,
		en_MSG_TYPE_LEAVE,
		en_MSG_TYPE_HEARTBEAT,
		en_MSG_TYPE_TERMINATE
	};

	struct st_MESSAGE
	{
		int			type;
		UINT64		clientKey;
		UINT64		requestID;
		RequestType	reqType;
		int			httpCode;
		WCHAR		json_req[1024];
		char		json_res[1024];
		int			reqLen;
		DWORD		requestTime;
	};

	HANDLE								_hMessageThread;
	CONDITION_VARIABLE					_cnMsgQ;
	cLockFreeQueue<st_MESSAGE*>			_msgQueue;
	SRWLOCK								_msgLock;
	cMemoryPool<st_MESSAGE, st_MESSAGE>		_msgPool;

private:
	CBattleSnake_Match_NetServer *		_pNetServer;
	cMemoryPool<st_REQUEST_INFO, st_REQUEST_INFO>	_requestInfoPool;
	std::list<st_REQUEST_INFO*>			_requestInfoList;

	WCHAR								_selectAccountURL[128];
	int									_selectAccountURL_Length;

public:
	UINT64								_monitor_msgPool;
	UINT64								_monitor_msgQueue;
	UINT64								_monitor_Info;
	UINT								_monitor_InfoPool;
	UINT64								_monitor_requestFail;
};