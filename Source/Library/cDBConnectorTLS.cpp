#include "cDBConnector.h"
#include "cDBConnectorTLS.h"

namespace TUNJI_LIBRARY
{
	cDBConnectorTLS::cDBConnectorTLS(WCHAR *dbIP, WCHAR *userName, WCHAR *password, WCHAR *dbName, int dbPort)
	{
		memcpy(_dbIP, dbIP, sizeof(WCHAR) * 16);
		memcpy(_userName, userName, sizeof(WCHAR) * 64);
		memcpy(_userPassword, password, sizeof(WCHAR) * 64);
		memcpy(_dbName, dbName, sizeof(WCHAR) * 64);
		_dbPort = dbPort;

		_dbMemoryPool = new cMemoryPool<cDBConnector, cDBConnector>;
		InitializeSRWLock(&_connectLock);
	}

	cDBConnectorTLS::~cDBConnectorTLS()
	{
		// 반환 하지 못한 연결까지 모두 Disconnect
		cDBConnector *pConnect = _dbMemoryPool->GetAllocInfo();

		while (pConnect != nullptr)
		{
			pConnect->Disconnect();
			_dbMemoryPool->Free(pConnect);
			InterlockedDecrement(&_dbAllocCount);

			pConnect = _dbMemoryPool->GetAllocInfo();
		}

		delete _dbMemoryPool;
	}

	cDBConnector* cDBConnectorTLS::Alloc(void)
	{
		cDBConnector *pConnect = nullptr;
		pConnect = (cDBConnector*)TlsGetValue(_tlsIndex);

		if (pConnect == NULL)
		{
			if ((_tlsIndex = TlsAlloc() == TLS_OUT_OF_INDEXES))
				return nullptr;

			pConnect = _dbMemoryPool->Alloc();
			TlsSetValue(_tlsIndex, pConnect);

			pConnect->SetDBConnect(_dbIP, _userName, _userPassword, _dbName, _dbPort);
			AcquireSRWLockExclusive(&_connectLock);
			bool ret = pConnect->Connect();
			ReleaseSRWLockExclusive(&_connectLock);
			if (ret == false)
				return nullptr;
			
			InterlockedIncrement(&_dbAllocCount);
		}

		return pConnect;
	}
	
	bool cDBConnectorTLS::FreeTlsIndex(void)
	{
		return TlsFree(_tlsIndex);
	}
};