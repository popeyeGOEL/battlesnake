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

		///	DB ���� 
		bool Connect(void);

		/// DB ���� 
		bool Disconnect(void);

		///	���� �� ��� �� �ӽ� ���� 
		bool Query(WCHAR *stringFormat, ...);
		
		/// ����� �������� ���� 
		///	DBWriter ������ Write ���� 
		bool QueryFromWriter(WCHAR *stringFormat, ...);

		///	���� �� ��� �������� 
		///	����� ���ٸ� NULL ���� 
		MYSQL_ROW FetchRow(void);

		///	������ ���� ��� ��� �� �������� ���� 
		void FreeResult(void);

		///	Error ��� 
		int DBGetLastError(void);
		char *DBGetLastErrorMsg(void);
		

		static cDBConnector* GetDBConnector(void);
		static bool			ConstructConnectorPool(WCHAR *dbIP, WCHAR *userName, WCHAR *password, WCHAR *dbName, int dbPort);
		static void			DestructConnectPool(void);

	private:
		///	Mysql�� LastError�� ��� ������ �����Ѵ�. 
		void SaveLastError(void);

		static cDBConnectorTLS *_dbTLS;

	private:
		

		MYSQL			_mysql;				///	MySql ���� ��ü
		MYSQL*			_pMysql;			///	���� ��ü ������ NULL ���η� ���� ���� Ȯ�� 
		MYSQL_RES*		_pQueryResult;		///	���� �� ��� ���� ������ 
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