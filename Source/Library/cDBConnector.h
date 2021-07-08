#pragma once
#include "mysql\include\mysql.h"
#include "mysql\include\errmsg.h"
#pragma comment(lib, "D:/VSproject/Tunji_Lib/mysql/lib/vs14/mysqlclient.lib")
//#pragma comment(lib, "C:/Users/user/Documents/Visual Studio 2017/Projects/Tunji_Lib/mysql/lib/vs14/mysqlclient.lib")
//C:\Users\user\Documents\Visual Studio 2017\Projects\Tunji_Lib\mysql

namespace TUNJI_LIBRARY
{
	class cDBConnectorTLS;
	class cDBConnector
	{
	public:

		enum DB_CONNECTOOR
		{
			en_QUERY_MAX_LEN = 2048
		};

		cDBConnector() {}
		//cDBConnector(WCHAR *dbIP, WCHAR *userName, WCHAR *password, WCHAR *dbName, int dbPort);
		virtual ~cDBConnector() {}

		void SetDBConnect(WCHAR *dbIP, WCHAR *userName, WCHAR *password, WCHAR *dbName, int dbPort);

		///	DB 연결 
		bool Connect(void);

		/// DB 끊기 
		bool Disconnect(void);

		///	쿼리 후 결과 셋 임시 보관 
		bool Query(WCHAR *stringFormat, ...);
		
		/// 결과셋 보관하지 않음 
		///	DBWriter 스레드 Write 전용 
		bool QueryFromWriter(WCHAR *stringFormat, ...);

		///	쿼리 후 결과 가져오기 
		///	결과가 없다면 NULL 리턴 
		MYSQL_ROW FetchRow(void);

		///	쿼리에 대한 결과 사용 후 마지막에 정리 
		void FreeResult(void);

		///	Error 얻기 
		int DBGetLastError(void);
		char *DBGetLastErrorMsg(void);
		

		static cDBConnector* GetDBConnector(void);
		static bool			ConstructConnectorPool(WCHAR *dbIP, WCHAR *userName, WCHAR *password, WCHAR *dbName, int dbPort);
		static void			DestructConnectPool(void);

	private:
		///	Mysql의 LastError를 멤버 변수로 저장한다. 
		void SaveLastError(void);

		static cDBConnectorTLS *_dbTLS;

	private:
		

		MYSQL			_mysql;				///	MySql 연결 객체
		MYSQL*			_pMysql;			///	연결 객체 포인터 NULL 여부로 연결 상태 확인 
		MYSQL_RES*		_pQueryResult;		///	쿼리 후 결과 보관 포인터 
		WCHAR			_dbIP[16];
		WCHAR			_userName[64];
		WCHAR			_userPassword[64];
		WCHAR			_dbName[64];
		int				_dbPort;

		WCHAR			_query[en_QUERY_MAX_LEN];
		char			_queryUTF8[en_QUERY_MAX_LEN];

		int				_lastError;
		char			_lastErrorMsg[128];
	};

};