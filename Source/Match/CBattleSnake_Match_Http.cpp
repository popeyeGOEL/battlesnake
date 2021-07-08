#include "CNetServer.h"
#include "CBattleSnake_Match_NetServer.h"

#include "cHttp_Async.h"
#include "CBattleSnake_Match_Http.h"

#include <process.h>

#include "CBattleSnake_Match_LocalMonitor.h"
#include "cSystemLog.h"
#include "cCrashDump.h"
#include "cThreadProfiler.h"

CBattleSnake_Match_Http::CBattleSnake_Match_Http(CBattleSnake_Match_NetServer *pNetServer)
{
	_monitor_requestFail = 0;
	_pNetServer = pNetServer;
	//InitializeSRWLock(&_requestInfoLock);
	InitializeSRWLock(&_msgLock);
	InitializeConditionVariable(&_cnMsgQ);

	CBattleSnake_Match_LocalMonitor *g_pMonitor = CBattleSnake_Match_LocalMonitor::GetInstance();
	g_pMonitor->SetMonitorFactor(this, CBattleSnake_Match_LocalMonitor::Factor_Match_Http);
}

CBattleSnake_Match_Http::~CBattleSnake_Match_Http()
{
	_requestInfoPool.ClearBuffer();
	_msgPool.ClearBuffer();
	_msgQueue.ClearBuffer();
}
void CBattleSnake_Match_Http::Start_MessageThread(void)
{
	DWORD threadID;
	_hMessageThread = (HANDLE)_beginthreadex(NULL, 0, MessageThread, (PVOID)this, 0, (unsigned int*)&threadID);
}
void CBattleSnake_Match_Http::Stop_MessageThread(void)
{
	st_MESSAGE *pMsg = _msgPool.Alloc();
	pMsg->type = en_MSG_TYPE_TERMINATE;
	_msgQueue.Enqueue(pMsg);
	WakeConditionVariable(&_cnMsgQ);

	DWORD status = WaitForSingleObject(_hMessageThread, INFINITE);
	CloseHandle(_hMessageThread);
}

void CBattleSnake_Match_Http::Send_Http_Heartbeat(void)
{
	static int prevTime = timeGetTime();
	int curTime = timeGetTime();

	if (curTime - prevTime < 10000) return;

	st_MESSAGE *pMsg = _msgPool.Alloc();
	pMsg->type = en_MSG_TYPE_HEARTBEAT;
	_msgQueue.Enqueue(pMsg);

	WakeConditionVariable(&_cnMsgQ);

	prevTime = curTime;
}

void CBattleSnake_Match_Http::Check_HeartBeat(void)
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

void CBattleSnake_Match_Http::OnLeaveServer(UINT64 requestID)
{
	st_MESSAGE *pMsg = _msgPool.Alloc();
	pMsg->type = en_MSG_TYPE_LEAVE;
	pMsg->requestID = requestID;
	_msgQueue.Enqueue(pMsg);

	WakeConditionVariable(&_cnMsgQ);
}

void CBattleSnake_Match_Http::OnRecv(UINT64 requestID, char* dsPacket, int dataLen, int httpCode)
{
	st_MESSAGE *pMsg = _msgPool.Alloc();
	pMsg->type = en_MSG_TYPE_RECV;
	pMsg->requestID = requestID;
	pMsg->httpCode = httpCode;
	memcpy(pMsg->json_res, dsPacket, sizeof(char) * dataLen);
	pMsg->json_res[dataLen] = '\0';
	_msgQueue.Enqueue(pMsg);
	WakeConditionVariable(&_cnMsgQ);
}

void CBattleSnake_Match_Http::Set_SelectAccountURL(WCHAR *selectAccountURL, int len)
{
	memcpy(_selectAccountURL, selectAccountURL, sizeof(WCHAR) * len);
	_selectAccountURL[len] = L'\0';
	_selectAccountURL_Length = len;
}

CBattleSnake_Match_Http::st_REQUEST_INFO* CBattleSnake_Match_Http::Alloc_RequestInfo(UINT64 clientKey)
{
	st_REQUEST_INFO *pInfo = _requestInfoPool.Alloc();
	pInfo->type = NONE;
	pInfo->clientKey = clientKey;
	pInfo->requestID = 0;
	pInfo->bRecv = false;
	pInfo->lastPacket = timeGetTime();

	_requestInfoList.push_back(pInfo);

	return pInfo;
}
void CBattleSnake_Match_Http::Free_RequestInfo(st_REQUEST_INFO *pInfo)
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

	LOG(L"MatchServer_Http", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Match_Http - Free_RequestInfo : CAN'T DELETE REQUEST INFO %d", pInfo->clientKey);
}

CBattleSnake_Match_Http::st_REQUEST_INFO* CBattleSnake_Match_Http::Find_RequestInfoByRequestID(UINT64 requestID)
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

	LOG(L"MatchServer_Http", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Match_Http - Find_RequestInfoByRequestID : CAN'T FIND REQUEST ID %d", requestID);
	return nullptr;
}


bool CBattleSnake_Match_Http::PacketProc(st_REQUEST_INFO *pInfo, char *dsPacket)
{
	switch (pInfo->type)
	{
	case SELECT_ACCOUNT:
		return Response_Select_Account(pInfo, dsPacket);
	default:
		return false;
	}
}

bool CBattleSnake_Match_Http::Send_Select_Account(UINT64 clientKey, UINT64 accountNo)
{
	//	JSON으로 Encoding
	st_MESSAGE *pMsg = _msgPool.Alloc();
	pMsg->type = en_MSG_TYPE_SEND;
	pMsg->clientKey = clientKey;
	pMsg->reqType = SELECT_ACCOUNT;
	pMsg->requestTime = timeGetTime();

	StringBuffer buffer;
	JsonEncode_Select_Account(buffer, accountNo);

	const char *pJson_utf8 = buffer.GetString();
	int jsonLen = strlen(pJson_utf8);
	pMsg->reqLen = jsonLen;
	int byte = MultiByteToWideChar(CP_UTF8, 0, pJson_utf8, jsonLen, pMsg->json_req, jsonLen);
	pMsg->json_req[byte] = L'\0';

	//LOG(L"MatchServer_Http", cSystemLog::LEVEL_DEBUG, L"#Request_Select_Account : Encode %s", pJson);

	_msgQueue.Enqueue(pMsg);
	WakeConditionVariable(&_cnMsgQ);

	return true;
}

bool CBattleSnake_Match_Http::Response_Select_Account(st_REQUEST_INFO *pInfo, char *dsPacket)
{
	//	Json으로 Decoding 한 결과를 NetServer에게 전달한다.
	int result;
	char sessionKey[64];
	JsonDecode_Select_Account(dsPacket, result, sessionKey);

	return _pNetServer->Receive_HttpRequest_SessionKey(pInfo->clientKey, result, sessionKey);
}

void CBattleSnake_Match_Http::JsonEncode_Select_Account(StringBuffer &buffer, UINT64 accountNo)
{
	Writer<StringBuffer, UTF16<>> writer(buffer);
	
	writer.StartObject();
	writer.String(L"accountno");
	writer.Uint64(accountNo);
	writer.EndObject();
}

void CBattleSnake_Match_Http::JsonDecode_Select_Account(char *pJson, int &result, char* sessionKey)
{
	Document Doc;
	Doc.Parse(pJson);

	Value & ResultObject = Doc["result"];
	result = ResultObject.GetInt();

	Value & SessionKeyObject = Doc["sessionkey"];
	if(!SessionKeyObject.Empty())
		memcpy(sessionKey, SessionKeyObject.GetString(), 64);
}

unsigned int CBattleSnake_Match_Http::MessageThread(void *arg)
{
	CBattleSnake_Match_Http *pThis = (CBattleSnake_Match_Http*)arg;
	return pThis->Update_MessageThread();
}
unsigned int CBattleSnake_Match_Http::Update_MessageThread(void)
{
	//	Request Thread 따로 만들어서,
	//	DirectRequestbyHttp를 한다. 만약 Alloc Session에 실패하면 다시 메시지 큐에 넣어놓는다. 
	st_MESSAGE *pMsg;
	st_REQUEST_INFO *pInfo;

	WCHAR *url = nullptr;
	int urlLen;

	cNetPacketPool::AllocPacketChunk();
	while (1)
	{
		AcquireSRWLockExclusive(&_msgLock);
		while (_msgQueue.IsEmpty())
		{
			SleepConditionVariableSRW(&_cnMsgQ, &_msgLock, INFINITE, 0);
		}

		_msgQueue.Dequeue(pMsg);

		switch (pMsg->type)
		{
		case en_MSG_TYPE_SEND:
		{
			UINT64 requestID;
			DWORD curTime = timeGetTime();
			if ( curTime - pMsg->requestTime < 20 || Alloc_Session(requestID) == false)
			{
				_msgQueue.Enqueue(pMsg);
				ReleaseSRWLockExclusive(&_msgLock);
				continue;
			}

			pInfo = Alloc_RequestInfo(pMsg->clientKey);
			pInfo->requestID = requestID;
			pInfo->type = pMsg->reqType;
			
			if (pInfo->type == SELECT_ACCOUNT)
			{
				url = _selectAccountURL;
				urlLen = _selectAccountURL_Length;
			}

			if (DirectRequestbyHttp(pInfo->requestID, url, urlLen, pMsg->json_req, pMsg->reqLen, MSG_TYPE::en_TYPE_POST) == false)
			{
				++_monitor_requestFail;
				LOG(L"MatchServer_Http", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Match_Http - Update_MessageThread : RequestHttp Failed %d", pMsg->requestID);
				_pNetServer->Fail_HttpRequest(pInfo->clientKey);
			}
			break;
		}
		case en_MSG_TYPE_RECV:
		{
			pInfo = Find_RequestInfoByRequestID(pMsg->requestID);
			if (pInfo == nullptr) break;

			pInfo->bRecv = true;

			if (pMsg->httpCode != 200)
			{
				++_monitor_requestFail;
				LOG(L"MatchServer_Http", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Match_Http - Update_MessageThread : Http Code Error %d", pMsg->requestID, pMsg->httpCode);
				_pNetServer->Fail_HttpRequest(pInfo->clientKey);
				break;
			}

			if (!PacketProc(pInfo, pMsg->json_res))
			{
				++_monitor_requestFail;
				LOG(L"MatchServer_Http", cSystemLog::LEVEL_ERROR, L"#CBattleSnake_Match_Http - Update_MessageThread : Packet Proc Error %d", pMsg->requestID);
				_pNetServer->Fail_HttpRequest(pInfo->clientKey);
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
					LOG(L"MatchServer_Http", cSystemLog::LEVEL_DEBUG, L"#CBattleSnake_Match_Http - Update_MessageThread : shutdown from web server %d", pMsg->requestID);
					_pNetServer->Fail_HttpRequest(pInfo->clientKey);
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
	};
	stop:
	cNetPacketPool::FreePacketChunk();
	return 0;
}