#include <Windows.h>
#include <strsafe.h>
#include "cDBConnectorTLS.h"
#include "cDBConnector.h"

namespace TUNJI_LIBRARY
{
	cDBConnectorTLS *cDBConnector::_dbTLS = 0;

	void cDBConnector::SetDBConnect(WCHAR *dbIP, WCHAR *userName, WCHAR *password, WCHAR *dbName, int dbPort)
	{
		_pMysql = NULL;
		_mysql = { 0, };
		memcpy(_dbIP, dbIP, sizeof(WCHAR) * 16);
		memcpy(_userName, userName, sizeof(WCHAR) * 64);
		memcpy(_userPassword, password, sizeof(WCHAR) * 64);
		memcpy(_dbName, dbName, sizeof(WCHAR) * 64);
		_dbPort = dbPort;

		memset(_query, 0, sizeof(WCHAR) * en_QUERY_MAX_LEN);
		memset(_queryUTF8, 0, sizeof(char) * en_QUERY_MAX_LEN);
		_lastError = 0;
		memset(_lastErrorMsg, 0, sizeof(char) * 128);
	}

	bool cDBConnector::Connect(void)
	{
		char dbIP[16] = { 0, };
		char userName[64] = { 0, };
		char userPassword[64] = { 0, };
		char dbName[64] = { 0, };
		int dbPort = _dbPort;

		MYSQL *conn = &_mysql;
		MYSQL **pConn = &_pMysql;

		WideCharToMultiByte(CP_UTF8, 0, _dbIP, 16, dbIP, 16, NULL, NULL);
		WideCharToMultiByte(CP_UTF8, 0, _userName, 64, userName, 64, NULL, NULL);
		WideCharToMultiByte(CP_UTF8, 0, _userPassword, 64, userPassword, 64, NULL, NULL);
		WideCharToMultiByte(CP_UTF8, 0, _dbName, 64, dbName, 64, NULL, NULL);

		///	Init
		mysql_init(conn);

		///	DB Connect 
		*pConn = mysql_real_connect(conn, dbIP, userName, userPassword, dbName, dbPort, (char*)NULL, 0);
		if (*pConn == NULL)
		{
			SaveLastError();
			return false;
		}

		mysql_set_character_set(*pConn, "utf8");
		return true;
	}

	bool cDBConnector::Disconnect(void)
	{
		mysql_close(_pMysql);

		if (_pMysql == NULL)
			return true;

		return false;
	}

	bool cDBConnector::Query(WCHAR *stringFormat, ...)
	{
		WCHAR *pQuery;
		char *pQueryUTF8;
		HRESULT hResult;
		va_list vl;
		int queryStatus;
		int reconnectionTry;
		bool bSuccess;

		pQuery = _query;
		bSuccess = true;

		//memset(_query, 0, sizeof(en_QUERY_MAX_LEN) * sizeof(WCHAR));
		//memset(_queryUTF8, 0, sizeof(char) * en_QUERY_MAX_LEN);
		_query[0] = L'\0';
		_queryUTF8[0] = L'\0';
		 
		va_start(vl, stringFormat);
		hResult = StringCchVPrintf(pQuery, en_QUERY_MAX_LEN, stringFormat, vl);
		va_end(vl);

		if (hResult == STRSAFE_E_INSUFFICIENT_BUFFER)
		{
			sprintf_s(_lastErrorMsg, "#ERROR #String is truncated, Insufficient Buffer\n");
			return false;
		}

		pQueryUTF8 = _queryUTF8;
		WideCharToMultiByte(CP_UTF8, 0, pQuery, en_QUERY_MAX_LEN, pQueryUTF8, en_QUERY_MAX_LEN, NULL, NULL);

		queryStatus = 0;
		reconnectionTry = 0;
		if (_pMysql != nullptr)
		{
			queryStatus = mysql_query(_pMysql, pQueryUTF8);
		}

		if (queryStatus != 0)
		{
			bSuccess = false;

			int err = mysql_errno(_pMysql);

			switch (err)
			{
			case CR_SOCKET_CREATE_ERROR:
			case CR_CONNECTION_ERROR:
			case CR_CONN_HOST_ERROR:
			case CR_SERVER_GONE_ERROR:
			case CR_SERVER_HANDSHAKE_ERR:
			case CR_SERVER_LOST:
			case CR_INVALID_CONN_HANDLE:
			{
				while (reconnectionTry < 5)
				{
					Sleep(100);
					Connect();
					if (_pMysql != nullptr)
					{
						bSuccess = true;
						queryStatus = mysql_query(_pMysql, pQueryUTF8);
						break;
					}
					++reconnectionTry;
				};
				break;
			}
			};
		}

		if (!bSuccess)
		{
			SaveLastError();
			return false;
		}

		//	결과셋 저장 
		if (_pMysql != nullptr)
		{
			_pQueryResult = mysql_store_result(_pMysql);
		}
		return true;
	}

	bool cDBConnector::QueryFromWriter(WCHAR *stringFormat, ...)
	{
		WCHAR *pQuery;
		char *pQueryUTF8;
		HRESULT hResult;
		va_list vl;
		int queryStatus;
		int reconnectionTry;
		bool bSuccess;

		pQuery = _query;
		bSuccess = true;
		va_start(vl, stringFormat);
		hResult = StringCchVPrintf(pQuery, en_QUERY_MAX_LEN, stringFormat, vl);
		va_end(vl);

		if (hResult == STRSAFE_E_INSUFFICIENT_BUFFER)
		{
			sprintf_s(_lastErrorMsg, "#ERROR #String is truncated, Insufficient Buffer\n");
			return false;
		}

		pQueryUTF8 = _queryUTF8;
		WideCharToMultiByte(CP_UTF8, 0, pQuery, en_QUERY_MAX_LEN, pQueryUTF8, en_QUERY_MAX_LEN, NULL, NULL);

		queryStatus = 0;
		reconnectionTry = 0;
		queryStatus = mysql_query(_pMysql, pQueryUTF8);

		if (queryStatus != 0)
		{
			bSuccess = false;

			int err = mysql_errno(_pMysql);

			switch (err)
			{
			case CR_SOCKET_CREATE_ERROR:
			case CR_CONNECTION_ERROR:
			case CR_CONN_HOST_ERROR:
			case CR_SERVER_GONE_ERROR:
			case CR_SERVER_HANDSHAKE_ERR:
			case CR_SERVER_LOST:
			case CR_INVALID_CONN_HANDLE:
			{
				while (reconnectionTry < 5)
				{
					Sleep(100);
					Connect();
					if (_pMysql != nullptr)
					{
						bSuccess = true;
						queryStatus = mysql_query(_pMysql, pQueryUTF8);
						break;
					}
					++reconnectionTry;
				};
				break;
			}
			};
		}

		if (!bSuccess)
		{
			SaveLastError();
			return false;
		}
		return true;
	}

	MYSQL_ROW cDBConnector::FetchRow(void)
	{
		return mysql_fetch_row(_pQueryResult);
	}

	void cDBConnector::FreeResult(void)
	{
		mysql_free_result(_pQueryResult);
	}

	int cDBConnector::DBGetLastError(void)
	{
		return _lastError;
	}

	char* cDBConnector::DBGetLastErrorMsg(void)
	{
		return _lastErrorMsg;
	}

	void cDBConnector::SaveLastError(void)
	{
		_lastError = mysql_errno(&_mysql);
		sprintf_s(_lastErrorMsg, "#ERROR : %s\n", mysql_error(&_mysql));
	}

	cDBConnector* cDBConnector::GetDBConnector(void)
	{
		return _dbTLS->Alloc();
	}

	bool cDBConnector::ConstructConnectorPool(WCHAR *dbIP, WCHAR *userName, WCHAR *password, WCHAR *dbName, int dbPort)
	{
		if (_dbTLS != nullptr) return false;
		_dbTLS = new cDBConnectorTLS(dbIP, userName, password, dbName, dbPort);
		return true;
	}

	void cDBConnector::DestructConnectPool(void)
	{
		if (_dbTLS->FreeTlsIndex())
		{
			delete _dbTLS;
			_dbTLS = nullptr;
		}
	}
};