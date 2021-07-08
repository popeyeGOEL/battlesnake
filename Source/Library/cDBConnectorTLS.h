#pragma once
#include "cMemoryPool_Log.h"
namespace TUNJI_LIBRARY
{
	class cDBConnector;
	class cDBConnectorTLS
	{
	public:
		cDBConnectorTLS(WCHAR *dbIP, WCHAR *userName, WCHAR *password, WCHAR *dbName, int dbPort);
		~cDBConnectorTLS();

		cDBConnector* Alloc(void);
		bool FreeTlsIndex(void);

	private:
		SRWLOCK			_connectLock;
		DWORD			_tlsIndex;
		cMemoryPool<cDBConnector, cDBConnector> *_dbMemoryPool;
		UINT64			_dbAllocCount;

		WCHAR			_dbIP[16];
		WCHAR			_userName[64];
		WCHAR			_userPassword[64];
		WCHAR			_dbName[64];
		int				_dbPort;
	};

};