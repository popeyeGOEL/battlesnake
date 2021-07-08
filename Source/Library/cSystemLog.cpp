#include <wchar.h>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <Strsafe.h>
#include <stdlib.h>
#include "cSystemLog.h"

namespace TUNJI_LIBRARY
{
	cSystemLog::cSystemLog()
	{
		memset(_dir, 0, sizeof(WCHAR) * 64);
		SetLogDirectory(L"log");

		_logLevel = en_LOG_LEVEL::LEVEL_DEBUG;
		_logCount = 0;

		_bPrintConsole = false;
		_bPrintText = false; 
		_bPrintDB = false;
		_bPrintWeb = false; 

		time_t timer;
		tm t;
		time(&timer);
		localtime_s(&t, &timer);
		wsprintf(_tableName, L"SystemServerLog_%04d%02d", t.tm_year + 1900, t.tm_mon + 1);
		InitializeSRWLock(&_fileLock);
	}

	cSystemLog::~cSystemLog()
	{
		DisconnectDB();
	}
	void cSystemLog::ConnectDB(void)
	{
		mysql_init(&_conn);
		_pConn = mysql_real_connect(&_conn, "127.0.0.1", "root", "0319", "server_log", 3306, (char*)NULL, 0);
		if (_pConn == NULL)
		{
			fprintf(stderr, "Mysql connection error: %s", mysql_error(&_conn));
			return;
		}

		mysql_set_character_set(_pConn, "utf8");

	}

	void cSystemLog::DisconnectDB(void)
	{
		mysql_close(_pConn);
	}
	void cSystemLog::SetLogDirectory(WCHAR *szDirectory)
	{
		if (wcslen(_dir) > 0)
		{
			_wrmdir(_dir);		//default로 생성된 디렉터리 삭제
		}

		wsprintf(_dir, L"%s", szDirectory);

		if (_wmkdir(_dir) == 0)
		{
			wprintf(L"Directory was sucessfully created!\n");
		}
		else
		{
			if (errno != EEXIST)
			{
				wprintf(L"Problem creating directory!\n");
			}
		}
	}

	void cSystemLog::SetLogLevel(en_LOG_LEVEL level)
	{
		_logLevel = level;
	}

	void cSystemLog::SetPrintMode(UINT mode)
	{
		UINT mask = 0x00000001;

		if ((mask & mode) == 1)
			_bPrintConsole = true;
		if ((mask & (mode >> 4)) == 1)
			_bPrintText = true;
		if ((mask & (mode >> 8)) == 1)
		{
			_bPrintDB = true;
			ConnectDB();
		}
		if ((mask & (mode >> 12)) == 1)
			_bPrintWeb = true;
	}

	void cSystemLog::ExecQuery(char *query)
	{
		int query_stat = 0;
		int reconnectLimit = 10;

		lockDB();
		query_stat = mysql_query(_pConn, query);
		unlockDB();

		int errDB = mysql_errno(_pConn);
		if (query_stat != 0)
		{
			wprintf(L"DB Disconnected!! Try to reconnect\n");
			switch (errDB)
			{
			case CR_SOCKET_CREATE_ERROR:
			case CR_CONNECTION_ERROR:
			case CR_CONN_HOST_ERROR:
			case CR_SERVER_GONE_ERROR:
			case CR_SERVER_HANDSHAKE_ERR:
			case CR_SERVER_LOST:
			case CR_INVALID_CONN_HANDLE:
			{
				while (reconnectLimit>0)
				{
					Sleep(100);
					ConnectDB();
					if (_pConn != nullptr)
					{
						printf("Reconnected to DB success\n");
						reconnectLimit = 10;
						break;
					}
					--reconnectLimit;
				}

				if (reconnectLimit == 10)
				{
					lockDB();
					query_stat = mysql_query(_pConn, query);
					unlockDB();
				}
				break;
			}
			case 1146:
			{
				char tableQuery[256];
				char tableName[32];
				WideCharToMultiByte(CP_UTF8, 0, _tableName, wcslen(_tableName), tableName, 256, NULL, NULL);

				sprintf_s(tableQuery, "create table `%s` like SystemServerLog_template", tableName);

				lockDB();
				mysql_query(_pConn, tableQuery);
				mysql_query(_pConn, query);
				unlockDB();
				break;
			}
			};
		}
	}

	void cSystemLog::Log(WCHAR *szType, en_LOG_LEVEL level, WCHAR *szStringFormat, ...)
	{
		if (_logLevel > level) return;

		errno_t err; 
		FILE *fp; 

		WCHAR fileName[128];
		WCHAR errLog[128];
		WCHAR logString[512];
		WCHAR result[640];
		WCHAR query[1024];
		int errLen = 0;
		time_t timer; 
		tm t; 
		HRESULT hResult;
		UINT64 count;

		count = InterlockedIncrement(&_logCount);
		time(&timer);
		localtime_s(&t, &timer);

		WCHAR logLevel[8] = { 0, };
		switch (level)
		{
		case LEVEL_DEBUG:
			wsprintf(logLevel, L"DEBUG");
			break;
		case LEVEL_WARNING:
			wsprintf(logLevel, L"WARNING");
			break;
		case LEVEL_ERROR:
			wsprintf(logLevel, L"ERROR");
			break;
		case LEVEL_SYSTEM:
			wsprintf(logLevel, L"SYSTEM");
			break;
		default:
			wsprintf(logLevel, L"ERROR");
			break;
		}

		// 가변인자 불러오기. 
		va_list vl; 
		va_start(vl, szStringFormat);
		hResult = StringCchVPrintf(logString, 512, szStringFormat, vl);
		va_end(vl);


		if (hResult == STRSAFE_E_INSUFFICIENT_BUFFER)
		{
			//	잘린 null terminated string이 들어있음 
			wsprintf(errLog, L"#ERROR #String is truncated, Insufficient Buffer!!\n");
			errLen = wcslen(errLog);
		}


		wsprintf(result, L"[%s][%04d-%02d-%02d %02d:%02d:%02d / %s / %010d] %s\n",	szType,
																					t.tm_year + 1900, 
																					t.tm_mon + 1, 
																					t.tm_mday, 
																					t.tm_hour, 
																					t.tm_min, 
																					t.tm_sec, 
																					logLevel, 
																					count,
																					logString);


		if (_bPrintConsole)
		{
			if (errLen > 0)
			{
				wprintf(L"%s", errLog);
			}
			wprintf(L"%s", result);
		}

		if (_bPrintText)
		{
			wsprintf(fileName, L"%s\\%04d%02d_%s.txt", _dir, t.tm_year + 1900, t.tm_mon + 1, szType);
			lockFile();
			err = _wfopen_s(&fp, fileName, L"at");
			if (err != 0)
			{
				wprintf(L"Fail to file open\n");
				return;
			}

			if (errLen > 0)
			{
				fwprintf_s(fp, errLog);
			}

			fwprintf_s(fp, result);
			fclose(fp);
			unlockFile();
		}

		if (_bPrintDB)
		{
			wsprintf(query, L"insert into `server_log`.`%s` (`server`, `date`, `logtype`, `logmode`, `accountno`, `log`) values ('%s', now(),'LOG', %d, %ld, '%s')",
					_tableName, szType, level, count, logString);

			char query_trans[1024]; 
			WideCharToMultiByte(CP_UTF8, 0, query, wcslen(query), query_trans, 1024, NULL, NULL);
			
			ExecQuery(query_trans);
		}
	}

	//	바이너리 출력(메모리)
	void cSystemLog::LogHex(WCHAR *szType, en_LOG_LEVEL level, WCHAR *szLog, BYTE *pByte, int iByteLen)
	{
		if (_logLevel > level || iByteLen == 0) return;

		errno_t err;
		FILE *fp;

		WCHAR fileName[128];
		WCHAR result[1024];
		WCHAR errLog[128];
		WCHAR query[1024];
		int errLen = 0;
		int idx = 0;
		int hexStartIdx = 0;
		time_t timer;
		tm t;
		UINT64 count;

		count = InterlockedIncrement(&_logCount);
		time(&timer);
		localtime_s(&t, &timer);

		WCHAR logLevel[8] = { 0, };
		switch (level)
		{
		case LEVEL_DEBUG:
			wsprintf(logLevel, L"DEBUG");
			break;
		case LEVEL_WARNING:
			wsprintf(logLevel, L"WARNING");
			break;
		case LEVEL_ERROR:
			wsprintf(logLevel, L"ERROR");
			break;
		case LEVEL_SYSTEM:
			wsprintf(logLevel, L"SYSTEM");
			break;
		default:
			wsprintf(logLevel, L"ERROR");
			break;
		}

		idx = wsprintf(result, L"[%s][%04d-%02d-%02d %02d:%02d:%02d / %s / %010d] ", szType,
			t.tm_year + 1900,
			t.tm_mon + 1,
			t.tm_mday,
			t.tm_hour,
			t.tm_min,
			t.tm_sec,
			logLevel,
			count);

		if (wcslen(szLog) > 1024 - idx)
		{
			wsprintf(errLog, L"#ERROR #String is truncated, Insufficient Buffer!!\n");
			errLen = wcslen(errLog);
		}
		else
		{
			idx += wsprintf(result + idx, szLog);

			hexStartIdx = idx; 

			if (iByteLen > 1024 - idx)
			{
				iByteLen = 1024 - idx;
				wsprintf(errLog, L"#ERROR #String is truncated, Insufficient Buffer!!\n");
				errLen = wcslen(errLog);
			}

			idx += wsprintf(result + idx, L" 0x%08x ", pByte);

			for (int i = 0; i < iByteLen; ++i)
			{
				idx += wsprintf(result + idx, L"%02x ", *(pByte + i));
			}
			wsprintf(result + idx, L"\n");

		}


		if (_bPrintConsole)
		{
			if (errLen > 0)
			{
				wprintf(L"%s", errLog);
			}
			wprintf(L"%s", result);
		}

		if (_bPrintText)
		{
			wsprintf(fileName, L"%s\\%04d%02d_%s.txt", _dir, t.tm_year + 1900, t.tm_mon + 1, szType);
			lockFile();
			err = _wfopen_s(&fp, fileName, L"at");
			if (err != 0)
			{
				wprintf(L"Fail to file open\n");
				return;
			}

			if (errLen > 0)
			{
				fwprintf_s(fp, errLog);
			}

			fwprintf_s(fp, result);
			fclose(fp);
			unlockFile();
		}


		if (_bPrintDB)
		{
			WCHAR hexString[1024];
			memcpy(hexString, result + hexStartIdx, sizeof(WCHAR) * (idx - hexStartIdx));

			wsprintf(query, L"insert into `server_log`.`%s` (`server`, `date`, `logtype`, `logmode`, `accountno`, `log`, `paramstring`) values ('%s', now(),'LOG_HEX', %d, %ld, '%s', '%s')",
				_tableName, szType, level, count, szLog, hexString);

			char query_trans[1024];
			WideCharToMultiByte(CP_UTF8, 0, query, wcslen(query), query_trans, 1024, NULL, NULL);

			ExecQuery(query_trans);
		}
	}
	
	// SessionKey 64개 출력 전용
	void cSystemLog::LogSessionKey(WCHAR *szType, en_LOG_LEVEL level, WCHAR *szLog, BYTE *pSessionKey)
	{

		if (_logLevel > level) return;

		//	문자열이 아니라서 마지막에 널이 없음.
		errno_t err;
		FILE *fp;

		WCHAR fileName[128];
		WCHAR result[1024];
		WCHAR errLog[128];
		WCHAR query[1024];
		int sessionStartIdx = 0;
		int errLen = 0;
		int idx = 0;
		time_t timer;
		tm t;
		UINT64 count;

		count = InterlockedIncrement(&_logCount);
		time(&timer);
		localtime_s(&t, &timer);

		WCHAR logLevel[8] = { 0, };
		switch (level)
		{
		case LEVEL_DEBUG:
			wsprintf(logLevel, L"DEBUG");
			break;
		case LEVEL_WARNING:
			wsprintf(logLevel, L"WARNING");
			break;
		case LEVEL_ERROR:
			wsprintf(logLevel, L"ERROR");
			break;
		case LEVEL_SYSTEM:
			wsprintf(logLevel, L"SYSTEM");
			break;
		default:
			wsprintf(logLevel, L"ERROR");
			break;
		}

		if (wcslen(szLog) > 900)
		{
			wsprintf(errLog, L"#ERROR #String is truncated, Insufficient Buffer!!\n");
			errLen = wcslen(errLog);
		}
		else
		{
			idx = wsprintf(result, L"[%s][%04d-%02d-%02d %02d:%02d:%02d / %s / %010d] %s ",		szType,
																								t.tm_year + 1900,
																								t.tm_mon + 1,
																								t.tm_mday,
																								t.tm_hour,
																								t.tm_min,
																								t.tm_sec,
																								logLevel,
																								count,
																								szLog);

			sessionStartIdx = idx; 

			for (int i = 0; i < 64; ++i)
			{
				idx += wsprintf(result + idx, L"%c", *(pSessionKey + i));
			}

			wsprintf(result + idx, L"\n");
		}

		if (_bPrintConsole)
		{
			if (errLen > 0)
			{
				wprintf(L"%s", errLog);
			}
			wprintf(L"%s", result);
		}

		if (_bPrintText)
		{
			wsprintf(fileName, L"%s\\%04d%02d_%s.txt", _dir, t.tm_year + 1900, t.tm_mon + 1, szType);
			lockFile();
			err = _wfopen_s(&fp, fileName, L"at");
			if (err != 0)
			{
				wprintf(L"Fail to file open\n");
				return;
			}

			if (errLen > 0)
			{
				fwprintf_s(fp, errLog);
			}

			fwprintf_s(fp, result);
			fclose(fp);
			unlockFile();
		}

		if (_bPrintDB)
		{
			WCHAR sessionString[64];
			memcpy(sessionString, result + sessionStartIdx, sizeof(WCHAR) * (idx - sessionStartIdx));

			wsprintf(query, L"insert into `server_log`.`%s` (`server`, `date`, `logtype`, `logmode`, `accountno`, `log`, `paramstring`) values ('%s', now(),'LOG_SESSION', %d, %ld, '%s', '%s')",
				_tableName, szType, level, count, szLog, sessionString);

			char query_trans[1024];
			WideCharToMultiByte(CP_UTF8, 0, query, wcslen(query), query_trans, 1024, NULL, NULL);

			ExecQuery(query_trans);
		}
	}

	void cSystemLog::lockFile(void)
	{
		AcquireSRWLockExclusive(&_fileLock);
	}

	void cSystemLog::unlockFile(void)
	{
		ReleaseSRWLockExclusive(&_fileLock);
	}

	void cSystemLog::lockDB(void)
	{
		AcquireSRWLockExclusive(&_dbLock);
	}

	void cSystemLog::unlockDB(void)
	{
		ReleaseSRWLockExclusive(&_dbLock);
	}
}