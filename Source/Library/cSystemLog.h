#pragma once
#include <Windows.h>
#include "mysql\include\mysql.h"
#include "mysql\include\errmsg.h"
//#pragma comment(lib, "D:/Win32/mysql/lib/vs14/mysqlclient.lib")
#pragma comment(lib, "D:/VSproject/Tunji_Lib/mysql/lib/vs14/mysqlclient.lib")
//#pragma comment(lib, "C:/Users/user/Documents/Visual Studio 2017/Projects/Tunji_Lib/mysql/lib/vs14/mysqlclient.lib")

namespace TUNJI_LIBRARY
{
#define dfPRINT_LOG_CONSOLE			0x00000001
#define dfPRINT_LOG_TEXT			0x00000010
#define dfPRINT_LOG_DB				0x00000100
#define dfPRINT_LOG_WEB				0x00001000

	class cSystemLog
	{
	public:
		enum en_LOG_LEVEL
		{
			LEVEL_DEBUG, LEVEL_WARNING, LEVEL_ERROR, LEVEL_SYSTEM
		};

		static cSystemLog* GetInstance()
		{
			static cSystemLog cLog;
			return &cLog;
		};

		virtual ~cSystemLog();
		
		void SetLogDirectory(WCHAR *szDirectory);
		void SetLogLevel(en_LOG_LEVEL level);
		void SetPrintMode(UINT mode);
	
		void Log(WCHAR *szType, en_LOG_LEVEL level, WCHAR *szStringFormat, ...);
		void LogHex(WCHAR *szType, en_LOG_LEVEL level, WCHAR *szLog, BYTE *pByte, int iByteLen);
		void LogSessionKey(WCHAR *szType, en_LOG_LEVEL level, WCHAR *szLog, BYTE *pSessionKey);
		
	protected:
		cSystemLog();
		void lockFile(void);
		void unlockFile(void);

		void lockDB(void);
		void unlockDB(void);

		void ConnectDB(void); 
		void DisconnectDB(void);
		void ExecQuery(char *query);

	private:
		WCHAR			_dir[64];
		en_LOG_LEVEL	_logLevel;
		UINT64			_logCount;
		bool			_bPrintConsole; 
		bool			_bPrintText; 
		bool			_bPrintDB; 
		bool			_bPrintWeb;		
		SRWLOCK			_fileLock;
		SRWLOCK			_dbLock;

		MYSQL			_conn;
		MYSQL*			_pConn;

		WCHAR			_tableName[32];
	};

#define SYSLOG_DIRECTORY(logDir)							\
	do{														\
		cSystemLog *g_pLog = cSystemLog::GetInstance();		\
		g_pLog->SetLogDirectory(logDir);					\
	}while(0)												\

#define SYSLOG_MODE(printMode)								\
	do{														\
		cSystemLog *g_pLog = cSystemLog::GetInstance();		\
		g_pLog->SetPrintMode(printMode);					\
	}while(0)												\

#define SYSLOG_LEVEL(logLevel)								\
	do{														\
		cSystemLog *g_pLog = cSystemLog::GetInstance();		\
		g_pLog->SetLogLevel(logLevel);						\
	}while(0)												\

#define LOG(Type, LogLevel, fmt, ...)						\
	do{														\
		cSystemLog *g_pLog = cSystemLog::GetInstance();		\
		g_pLog->Log(Type, LogLevel, fmt,  ##__VA_ARGS__);	\
	}while(0)												\

#define LOG_HEX(Type, LogLevel, szLog, pByte, len)				\
	do{															\
		cSystemLog *g_pLog  = cSystemLog::GetInstance();		\
		g_pLog->LogHex(Type, LogLevel, szLog, pByte, len);		\
	} while (0)													\

}