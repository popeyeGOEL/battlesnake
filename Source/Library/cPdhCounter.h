#pragma once
#include <Pdh.h>
#include <PdhMsg.h>
#include <strsafe.h>
namespace TUNJI_LIBRARY
{
#define df_PDH_ETHERNET_MAX	8

	class cPdhCounter
	{
	public:
		cPdhCounter();
		bool InitEthernetCounter(void);
		bool InitProcessCounter(WCHAR *szProcess);
		bool InitMemoryCounter(void);

		void UpdatePdhQueryData(void);

		void GetNetworkStatus(double &recv, double &send);
		void GetProcessStatus(double &commitByte);
		void GetMemoryStatus(double &availableByte, double &nonPagedBytes);

		void TerminatePdhCounter(void);
		
	private:

		//	이더넷 하나에 대한 Send, Recv PDH 정보
		struct st_ETHERNET
		{
			st_ETHERNET()
			{
				_bUsed = false;
				memset(_name, 0, sizeof(WCHAR) * 128);
				_pdh_counter_network_recvBytes = 0;
				_pdh_counter_network_sendBytes = 0;
			}

			bool			_bUsed;
			WCHAR			_name[128];

			PDH_HCOUNTER	_pdh_counter_network_recvBytes;
			PDH_HCOUNTER	_pdh_counter_network_sendBytes;
		};

		struct st_PROCESS
		{
			st_PROCESS()
			{
				memset(_name, 0, sizeof(WCHAR) * 128);
				_pdh_counter_process_privateBytes = 0;
			}
			WCHAR _name[128];
			PDH_HCOUNTER _pdh_counter_process_privateBytes;
		};

		struct st_MEMORY
		{
			st_MEMORY()
			{
				_pdh_counter_memory_availableMemoryMBytes = 0;
				_pdh_counter_memory_nonPagedMemoryMBytes = 0;
			}
			PDH_HCOUNTER	_pdh_counter_memory_availableMemoryMBytes;
			PDH_HCOUNTER	_pdh_counter_memory_nonPagedMemoryMBytes;
		};

		st_ETHERNET	_ethernetStruct[df_PDH_ETHERNET_MAX];	//	랜 카드별 PDH 정보
		st_PROCESS	_processStruct;
		st_MEMORY	_memoryStruct;
		PDH_HQUERY	_pdh_query;

		bool		_bInitEthernet;
		bool		_bInitProcess;
		bool		_bInitMemory;
	};

};