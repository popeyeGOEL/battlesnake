#include "CNetSession.h"
#include "CBattleSnake_BattlePlayer_NetSession.h"

#include "CMMOServer.h"
#include "CBattleSnake_Battle_MMOServer.h"

#include "cHttp_Async.h"
#include "CBattleSnake_Battle_Http.h"

#include <process.h>

#include "CBattleSnake_Battle_LocalMonitor.h"
#include "cSystemLog.h"
#include "cThreadProfiler.h"
#include "cCrashDump.h"

CBattleSnake_Battle_Http *g_pHttp = nullptr;

CBattleSnake_Battle_Http::CBattleSnake_Battle_Http()
{
	_monitor_requestFail = 0;
	InitializeSRWLock(&_msgLock);
	InitializeConditionVariable(&_cnMsgQueueIsNotEmpty);

	CBattleSnake_Battle_LocalMonitor *g_pMonitor = CBattleSnake_Battle_LocalMonitor::GetInstance();
	g_pMonitor->SetMonitorFactor(this, CBattleSnake_Battle_LocalMonitor::Factor_Battle_Http);
}

CBattleSnake_Battle_Http::~CBattleSnake_Battle_Http()
{
	_msgQueue.ClearBuffer();
	_requestInfoPool.ClearBuffer();
	_msgPool.ClearBuffer();
}

void CBattleSnake_Battle_Http::Start_MessageThread(void)
{
	DWORD threadID;
	_hMessageThread = (HANDLE)_beginthreadex(NULL, 0, MessageThread, (PVOID)this, 0, (unsigned int*)&threadID);
}
void CBattleSnake_Battle_Http::Stop_MessageThread(void)
{
	st_MESSAGE *pMsg = _msgPool.Alloc();
	pMsg->type = en_MSG_TYPE_TERMINATE;
	_msgQueue.Enqueue(pMsg);
	WakeConditionVariable(&_cnMsgQueueIsNotEmpty);

	DWORD status = WaitForSingleObject(_hMessageThread, INFINITE);
	CloseHandle(_hMessageThread);
}

void CBattleSnake_Battle_Http::Send_Http_Heartbeat(void)
{
	static int prevTime = timeGetTime();
	int curTime = timeGetTime();

	if (curTime - prevTime < 10000) return;

	st_MESSAGE *pMsg = _msgPool.Alloc();
	pMsg->type = en_MSG_TYPE_HEARTBEAT;
	_msgQueue.Enqueue(pMsg);

	WakeConditionVariable(&_cnMsgQueueIsNotEmpty);

	prevTime = curTime;
}

void CBattleSnake_Battle_Http::OnLeaveServer(UINT64 requestID)
{
	st_MESSAGE *pMsg = _msgPool.Alloc();
	pMsg->type = en_MSG_TYPE_LEAVE;
	pMsg->requestID = requestID;
	_msgQueue.Enqueue(pMsg);

	WakeConditionVariable(&_cnMsgQueueIsNotEmpty);
}

void CBattleSnake_Battle_Http::OnRecv(UINT64 requestID, char* dsPacket, int dataLen, int httpCode)
{
	st_MESSAGE *pMsg = _msgPool.Alloc();
	pMsg->type = en_MSG_TYPE_RECV;
	pMsg->requestID = requestID;
	pMsg->httpCode = httpCode;
	memcpy(pMsg->json_res, dsPacket, sizeof(char) * dataLen);
	pMsg->json_res[dataLen] = '\0';
	_msgQueue.Enqueue(pMsg);
	WakeConditionVariable(&_cnMsgQueueIsNotEmpty);
}

void CBattleSnake_Battle_Http::Set_SelectAccountURL(WCHAR *selectAccountURL, int len)
{
	memcpy(_selectAccountURL, selectAccountURL, sizeof(WCHAR) * len);
	_selectAccountURL[len] = L'\0';
	_selectAccountURL_Length = len;
}

void CBattleSnake_Battle_Http::Set_SelectContentsURL(WCHAR* selectContentsURL, int len)
{
	memcpy(_selectContentsURL, selectContentsURL, sizeof(WCHAR) * len);
	_selectContentsURL[len] = L'\0';
	_selectContentsURL_Length = len;
}
void CBattleSnake_Battle_Http::Set_UpdateContentsURL(WCHAR* updateContentsURL, int len)
{
	memcpy(_updateContentsURL, updateContentsURL, sizeof(WCHAR) * len);
	_updateContentsURL[len] = L'\0';
	_updateContentsURL_Length = len;
}


CBattleSnake_Battle_Http::st_REQUEST_INFO* CBattleSnake_Battle_Http::Alloc_RequestInfo(UINT64 clientID)
{
	st_REQUEST_INFO *pInfo = _requestInfoPool.Alloc();
	pInfo->type = NONE;
	pInfo->requestID = 0;
	pInfo->clientID = clientID;
	pInfo->bRecv = false;
	pInfo->lastPacket = timeGetTime();

	_requestInfoList.push_back(pInfo);

	return pInfo;
}
void CBattleSnake_Battle_Http::Free_RequestInfo(st_REQUEST_INFO *pInfo)
{
	std::list<st_REQUEST_INFO*>::iterator iter;
	std::list<st_REQUEST_INFO*> *pList = &_requestInfoList;

	iter = pList->begin();
	while (iter != pList->end())
	{
		if (*iter == pInfo)
		{
			pList->erase(iter);
			_requestInfoPool.Free(pInfo);
			return;
		}
		++iter;
	}

	LOG(L"BattleServer_Http", cSystemLog::LEVEL_ERROR, L"#Free_RequestInfo : CAN'T DELETE REQUEST INFO %d", pInfo->requestID);
}

CBattleSnake_Battle_Http::st_REQUEST_INFO* CBattleSnake_Battle_Http::Find_RequestInfoByRequestID(UINT64 requestID)
{
	std::list<st_REQUEST_INFO*>::iterator iter;
	std::list<st_REQUEST_INFO*> *pList = &_requestInfoList;
	st_REQUEST_INFO *pInfo = nullptr;

	iter = pList->begin();
	while (iter != pList->end())
	{
		pInfo = *iter;
		if (pInfo->requestID == requestID)
		{
			return pInfo;
		}
		++iter;
	}

	LOG(L"BattleServer_Http", cSystemLog::LEVEL_ERROR, L"#Find_RequestInfoByRequestID : CAN'T FIND REQUEST ID %d", requestID);
	return nullptr;
}

bool CBattleSnake_Battle_Http::PacketProc(st_REQUEST_INFO* pInfo, char* dsPacket)
{
	switch (pInfo->type)
	{
	case SELECT_ACCOUNT:
		return Response_Select_Account(pInfo, dsPacket);
	case SELECT_CONTENTS:
		return Response_Select_Contents(pInfo, dsPacket);
	case UPDATE_CONTENTS:
		return Response_Update_Contents(pInfo, dsPacket);
	default:
		return false;
	};
}
bool CBattleSnake_Battle_Http::Send_Select_Account(UINT64 clientID, UINT64 accountNo)
{
	st_MESSAGE *pMsg = _msgPool.Alloc();
	pMsg->type = en_MSG_TYPE_SEND;
	pMsg->clientID = clientID;
	pMsg->reqType = SELECT_ACCOUNT;
	pMsg->requestTime = timeGetTime();

	StringBuffer buffer;
	JsonEncode_AccountNo(buffer, accountNo);

	const char* pJson_utf8 = buffer.GetString();
	int jsonLen = strlen(pJson_utf8);
	pMsg->reqLen = jsonLen;

	int byte = MultiByteToWideChar(CP_UTF8, 0, pJson_utf8, jsonLen, pMsg->json_req, jsonLen);
	pMsg->json_req[byte] = L'\0';

	_msgQueue.Enqueue(pMsg);
	WakeConditionVariable(&_cnMsgQueueIsNotEmpty);

	return true;
}

bool CBattleSnake_Battle_Http::Send_Select_Contents(UINT64 clientID, UINT64 accountNo)
{
	st_MESSAGE *pMsg = _msgPool.Alloc();
	pMsg->type = en_MSG_TYPE_SEND;
	pMsg->clientID = clientID;
	pMsg->reqType = SELECT_CONTENTS;
	pMsg->requestTime = timeGetTime();

	StringBuffer buffer;
	JsonEncode_AccountNo(buffer, accountNo);

	const char* pJson_utf8 = buffer.GetString();
	int jsonLen = strlen(pJson_utf8);
	pMsg->reqLen = jsonLen;

	int byte = MultiByteToWideChar(CP_UTF8, 0, pJson_utf8, jsonLen, pMsg->json_req, jsonLen);
	pMsg->json_req[byte] = L'\0';

	_msgQueue.Enqueue(pMsg);
	WakeConditionVariable(&_cnMsgQueueIsNotEmpty);
	return true;
}

bool CBattleSnake_Battle_Http::Send_Update_Contents(UINT64 clientID, UINT64 accountNo, stRecord &record)
{
	st_MESSAGE *pMsg = _msgPool.Alloc();
	pMsg->type = en_MSG_TYPE_SEND;
	pMsg->clientID = clientID;
	pMsg->reqType = UPDATE_CONTENTS;
	pMsg->requestTime = timeGetTime();

	StringBuffer buffer;
	JsonEncode_Contents(buffer, accountNo, record);

	const char* pJson_utf8 = buffer.GetString();
	int jsonLen = strlen(pJson_utf8);
	pMsg->reqLen = jsonLen;

	int byte = MultiByteToWideChar(CP_UTF8, 0, pJson_utf8, jsonLen, pMsg->json_req, jsonLen);
	pMsg->json_req[byte] = L'\0';

	_msgQueue.Enqueue(pMsg);
	WakeConditionVariable(&_cnMsgQueueIsNotEmpty);

	return true;
}

bool CBattleSnake_Battle_Http::Response_Select_Account(st_REQUEST_INFO *pInfo, char *dsPacket)
{
	int result;
	WCHAR nickName[20] = { 0, };
	char sessionKey[64];

	JsonDecode_Select_Account(dsPacket, result, nickName, sessionKey);

	CBattleSnake_BattlePlayer_NetSession *pPlayer = g_pBattle->Find_PlayerByClientID(pInfo->clientID);
	if (pPlayer == nullptr) return false;

	pPlayer->Receive_HttpRequest_SearchAccount(result, nickName, sessionKey);
	return true;
}
bool CBattleSnake_Battle_Http::Response_Select_Contents(st_REQUEST_INFO *pInfo, char *dsPacket)
{
	int result;
	stRecord record;

	JsonDecode_Select_Contents(dsPacket, result, record);

	CBattleSnake_BattlePlayer_NetSession *pPlayer = g_pBattle->Find_PlayerByClientID(pInfo->clientID);
	if (pPlayer == nullptr) return false;

	pPlayer->Receive_HttpRequest_SearchContents(result, record);
	return true;
}

bool CBattleSnake_Battle_Http::Response_Update_Contents(st_REQUEST_INFO *pInfo, char *dsPacket)
{
	int result;

	JsonDecode_Update_Contents(dsPacket, result);

	CBattleSnake_BattlePlayer_NetSession *pPlayer = g_pBattle->Find_PlayerByClientID(pInfo->clientID);
	if (pPlayer == nullptr) return false;

	pPlayer->Receive_HttpRequest_UpdateContents(result);
	return true;
}

void CBattleSnake_Battle_Http::JsonEncode_AccountNo(StringBuffer &buffer, UINT64 accountNo)
{
	Writer<StringBuffer, UTF16<>> writer(buffer);

	writer.StartObject();
	writer.String(L"accountno");
	writer.Uint64(accountNo);
	writer.EndObject();
}

void CBattleSnake_Battle_Http::JsonEncode_Contents(StringBuffer &buffer, UINT64 accountNo, stRecord &record)
{
	//	-1이 아닌 값만 보내기.
	Writer<StringBuffer, UTF16<>> writer(buffer);

	writer.StartObject();
	writer.String(L"accountno");
	writer.Uint64(accountNo);

	writer.String(L"playtime");
	writer.Uint(record.playTime);

	writer.String(L"playcount");
	writer.Uint(record.playCount);

	writer.String(L"kill");
	writer.Uint(record.kill);

	writer.String(L"die");
	writer.Uint(record.die);

	writer.String(L"win");
	writer.Uint(record.win);
	writer.EndObject();
}

void CBattleSnake_Battle_Http::JsonDecode_Select_Account(char* pJson, int &result, WCHAR* nickname, char* sessionKey)
{
	Document doc;
	doc.Parse(pJson);

	Value & resultObject = doc["result"];
	result = resultObject.GetInt();

	Value & nickObject = doc["nick"];
	if (!nickObject.Empty())
	{
		int byte = MultiByteToWideChar(CP_UTF8, 0, nickObject.GetString(), nickObject.GetStringLength(), nickname, 20);
	}

	Value & sessionKeyObejct = doc["sessionkey"];
	if (!sessionKeyObejct.Empty())
	{
		memcpy(sessionKey, sessionKeyObejct.GetString(), sizeof(char) * 64);
	}

}
void CBattleSnake_Battle_Http::JsonDecode_Select_Contents(char* pJson, int &result, stRecord &record)
{
	Document doc;
	doc.Parse(pJson);

	Value & resultObject = doc["result"];
	result = resultObject.GetInt();

	Value & playTimeObject = doc["playtime"];
	record.playTime = playTimeObject.GetInt();

	Value & playCountObject = doc["playcount"];
	record.playCount = playCountObject.GetInt();

	Value & killObject = doc["kill"];
	record.kill = killObject.GetInt();

	Value & dieObject = doc["die"];
	record.die = dieObject.GetInt();

	Value & winObject = doc["win"];
	record.win = winObject.GetInt();
}

void CBattleSnake_Battle_Http::JsonDecode_Update_Contents(char* pJson, int& result)
{
	Document doc;
	doc.Parse(pJson);

	Value & resultObject = doc["result"];
	result = resultObject.GetInt();
}

unsigned int CBattleSnake_Battle_Http::MessageThread(void * arg)
{
	CBattleSnake_Battle_Http *pThis = (CBattleSnake_Battle_Http*)arg;
	return pThis->Update_MessageThread();
}

unsigned int CBattleSnake_Battle_Http::Update_MessageThread(void)
{
	st_MESSAGE *pMsg;
	st_REQUEST_INFO *pInfo;

	WCHAR *url;
	int urlLen;

	cNetPacketPool::AllocPacketChunk();

	while (1)
	{
		AcquireSRWLockExclusive(&_msgLock);
		while (_msgQueue.IsEmpty())
		{
			SleepConditionVariableSRW(&_cnMsgQueueIsNotEmpty, &_msgLock, INFINITE, 0);
		}

		_msgQueue.Dequeue(pMsg);

		switch (pMsg->type)
		{
		case en_MSG_TYPE_SEND:
		{			
			if (g_pBattle->Find_PlayerByClientID(pMsg->clientID) == nullptr) break;

			UINT64 requestID;
			if ((timeGetTime() - pMsg->requestTime < 20) || (Alloc_Session(requestID) == false))
			{
				_msgQueue.Enqueue(pMsg);
				ReleaseSRWLockExclusive(&_msgLock);
				continue;
			}

			pInfo = Alloc_RequestInfo(pMsg->clientID);
			pInfo->requestID = requestID;
			pInfo->type = pMsg->reqType;

			switch (pMsg->reqType)
			{
			case SELECT_ACCOUNT:
				url = _selectAccountURL;
				urlLen = _selectAccountURL_Length;
				break;
			case SELECT_CONTENTS:
				url = _selectContentsURL;
				urlLen = _selectContentsURL_Length;
				break;
			case UPDATE_CONTENTS:
				url = _updateContentsURL;
				urlLen = _updateContentsURL_Length;
				break;
			}

			if (DirectRequestbyHttp(pInfo->requestID, url, urlLen, pMsg->json_req, pMsg->reqLen, MSG_TYPE::en_TYPE_POST) == false)
			{
				++_monitor_requestFail;
				LOG(L"BattleServer_Http", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Battle_Http Update_MessageThread : RequestHttp Failed %d", pMsg->requestID);

				//	Fail_HttpRequest
				CBattleSnake_BattlePlayer_NetSession *pPlayer = g_pBattle->Find_PlayerByClientID(pInfo->clientID);
				if (pPlayer == nullptr) break;
				pPlayer->Fail_HttpRequest(pInfo->clientID, pInfo->type);
			}

			break;
		}
		case en_MSG_TYPE_RECV:
		{
			pInfo = Find_RequestInfoByRequestID(pMsg->requestID);
			if (pInfo == nullptr)
			{
				break;
			}
			
			pInfo->bRecv = true;

			if (pMsg->httpCode != 200)
			{
				++_monitor_requestFail;

				LOG(L"BattleServer_Http", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Battle_Http Update_MessageThread : Http Code Error %d", pMsg->requestID, pMsg->httpCode);
				//	Fail_HttpRequest
				CBattleSnake_BattlePlayer_NetSession *pPlayer = g_pBattle->Find_PlayerByClientID(pInfo->clientID);
				if (pPlayer == nullptr) break;

				pPlayer->Fail_HttpRequest(pInfo->clientID, pInfo->type);
				break;
			}

			if (!PacketProc(pInfo, pMsg->json_res))
			{
				++_monitor_requestFail;
				LOG(L"BattleServer_Http", cSystemLog::LEVEL_DEBUG, L"#CBattleSnake_Battle_Http Update_MessageThread : Packet Proc Error %s", pMsg->json_res);

				//	Fail_HttpRequest
				CBattleSnake_BattlePlayer_NetSession *pPlayer = g_pBattle->Find_PlayerByClientID(pInfo->clientID);
				if (pPlayer == nullptr) break;

				pPlayer->Fail_HttpRequest(pInfo->clientID, pInfo->type);
			}

			break;
		}
		case en_MSG_TYPE_LEAVE:
		{
			pInfo = Find_RequestInfoByRequestID(pMsg->requestID);
			if (pInfo != nullptr)
			{
				if (pInfo->bRecv == false)
				{
					++_monitor_requestFail;
					LOG(L"BattleServer_Http", cSystemLog::LEVEL_DEBUG, L"#CBattleSnake_Battle_Http Update_MessageThread : shutdown from web server %d", pMsg->requestID);
					//	Fail_HttpRequest
					CBattleSnake_BattlePlayer_NetSession *pPlayer = g_pBattle->Find_PlayerByClientID(pInfo->clientID);
					if (pPlayer != nullptr)
					{
						pPlayer->Fail_HttpRequest(pInfo->clientID, pInfo->type);
					}
				}

				Free_RequestInfo(pInfo);
			}
			break;
		}

		case en_MSG_TYPE_HEARTBEAT:
		{
			Check_HeartBeat();
			break;
		}
		case en_MSG_TYPE_TERMINATE:
		{
			_msgPool.Free(pMsg);
			ReleaseSRWLockExclusive(&_msgLock);
			goto stop;
		}
		};

		_monitor_msgPool = _msgQueue.GetTotalBlockCount();
		_monitor_msgQueue = _msgQueue.GetUsedBlockCount();
		_monitor_Info = _requestInfoPool.GetUsedBlockCount();
		_monitor_InfoPool = _requestInfoPool.GetTotalBlockCount();
		_msgPool.Free(pMsg);
		ReleaseSRWLockExclusive(&_msgLock);
		Sleep(0);
	};

stop:
	cNetPacketPool::FreePacketChunk();
	return 0;
}

void CBattleSnake_Battle_Http::Check_HeartBeat(void)
{
	std::list<st_REQUEST_INFO*> *pList = &_requestInfoList;
	std::list<st_REQUEST_INFO*>::iterator iter = pList->begin();
	st_REQUEST_INFO* pInfo = nullptr;

	int curTime = timeGetTime();
	while (iter != pList->end())
	{
		pInfo = *iter;
		if (curTime - pInfo->lastPacket > 20000)
		{
			Free_Session(pInfo->requestID);
		}
		++iter;
	}
}



