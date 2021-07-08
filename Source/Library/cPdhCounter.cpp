#include <Windows.h>
#include "cPdhCounter.h"

#pragma comment(lib, "pdh.lib")

namespace TUNJI_LIBRARY
{
	cPdhCounter::cPdhCounter()
	{
		PdhOpenQuery(NULL, 0, &_pdh_query);
		_bInitEthernet = false;
		_bInitProcess = false;
	}
	void cPdhCounter::UpdatePdhQueryData(void)
	{
		PdhCollectQueryData(_pdh_query);
	}

	bool cPdhCounter::InitEthernetCounter(void)
	{
		int count = 0;
		bool bError = false;
		WCHAR *szCur = nullptr;
		WCHAR *szCounters = nullptr;
		WCHAR *szInterfaces = nullptr;
		DWORD dwCounterSize = 0, dwInterfaceSize = 0;
		WCHAR szQuery[1024] = { 0, };


		//	PDH enum object�� ���
		//	��� �̴��� �̸��� �������� ���� ������� �̴���, �����̴��� ����� Ȯ�� �Ұ� 

		//	PDHEnumOjbectItems �� "NetworkInterface" �׸񿡼� ���� �� �ִ� 
		//	�����׸�(counters) / �������̽� �׸�(Interfaces)�� ����. �׷��� �� ������ ���̴� �𸣱� ������
		//	���� ������ ���̸� �˱� ���ؼ� Out Buffer ���ڵ��� NULL �����ͷ� �־ ����� Ȯ���Ѵ�. 

		PdhEnumObjectItems(NULL, NULL, L"Network Interface", szCounters, &dwCounterSize, szInterfaces, &dwInterfaceSize, PERF_DETAIL_WIZARD, 0);
		szCounters = new WCHAR[dwCounterSize];
		szInterfaces = new WCHAR[dwInterfaceSize];

		//	������ ���� �Ҵ� �� �� ȣ�� 
		//	szCounters�� szInterfaces ���ۿ��� �������� ���ڿ��� ���� 
		//	NULL �����ͷ� ������ ���ڿ����� dwCounterSize, dwInterfaceSize ���̸�ŭ ���� 
		//	���ڿ� ������ ��� ������ Ȯ�� 

		if (PdhEnumObjectItems(NULL, NULL, L"Network Interface", szCounters, &dwCounterSize, szInterfaces, &dwInterfaceSize, PERF_DETAIL_WIZARD, 0) != ERROR_SUCCESS)
		{
			delete[] szCounters;
			delete[] szInterfaces;
			return false;
		}

		count = 0;
		szCur = szInterfaces;

		//	szInterfaces���� ���ڿ� ������ �����鼭 �̸��� ���� �޴´� 

		for(;*szCur != L'\0'  && count < df_PDH_ETHERNET_MAX; szCur += wcslen(szCur) + 1, ++count)
		{
			_ethernetStruct[count]._bUsed = true;
			_ethernetStruct[count]._name[0] = L'\0';
			
			wcscpy_s(_ethernetStruct[count]._name, szCur);

			szQuery[0] = L'\0';
			StringCbPrintf(szQuery, sizeof(WCHAR) * 1024, L"\\Network Interface(%s)\\Bytes Received/sec", szCur);
			PdhAddCounter(_pdh_query, szQuery, NULL, &_ethernetStruct[count]._pdh_counter_network_recvBytes);

			szQuery[0] = L'\0';
			StringCbPrintf(szQuery, sizeof(WCHAR) * 1024, L"\\Network Interface(%s)\\Bytes Sent/sec", szCur);
			PdhAddCounter(_pdh_query, szQuery, NULL, &_ethernetStruct[count]._pdh_counter_network_sendBytes);
		}

		_bInitEthernet = true;
		return true;
	}

	bool cPdhCounter::InitProcessCounter(WCHAR *szProcess)
	{
		WCHAR szQuery[1024] = { 0, };

		_processStruct._name[0] = L'\0';
		wcscpy_s(_processStruct._name, szProcess);

		szQuery[0] = L'\0';
		StringCbPrintf(szQuery, sizeof(WCHAR) * 1024, L"\\Process(%s)\\Private Bytes", szProcess);
		PdhAddCounter(_pdh_query, szQuery, NULL, &_processStruct._pdh_counter_process_privateBytes);

		_bInitProcess = true;
		return true;
	}

	bool cPdhCounter::InitMemoryCounter(void)
	{
		bool bError = false;
		WCHAR szQuery[1024] = { 0, };

		szQuery[0] = L'\0';
		StringCbPrintf(szQuery, sizeof(WCHAR) * 1024, L"\\Memory\\Available MBytes");
		PdhAddCounter(_pdh_query, szQuery, NULL, &_memoryStruct._pdh_counter_memory_availableMemoryMBytes);

		szQuery[0] = L'\0';
		StringCbPrintf(szQuery, sizeof(WCHAR) * 1024, L"\\Memory\\Pool Nonpaged Bytes");
		PdhAddCounter(_pdh_query, szQuery, NULL, &_memoryStruct._pdh_counter_memory_nonPagedMemoryMBytes);

		_bInitMemory = true;
		return true;
	}

	void cPdhCounter::GetNetworkStatus(double &recv, double &send)
	{
		PDH_STATUS status;
		PDH_FMT_COUNTERVALUE counterValue;

		if (!_bInitEthernet) return;

		recv = 0;
		send = 0;

		for(int i =0 ; i < df_PDH_ETHERNET_MAX; ++i)
		{
			if(_ethernetStruct[i]._bUsed)
			{
				status = PdhGetFormattedCounterValue(_ethernetStruct[i]._pdh_counter_network_recvBytes, PDH_FMT_DOUBLE, NULL, &counterValue);
				if (status == 0) recv += counterValue.doubleValue;
				status = PdhGetFormattedCounterValue(_ethernetStruct[i]._pdh_counter_network_sendBytes, PDH_FMT_DOUBLE, NULL, &counterValue);
				if (status == 0) send += counterValue.doubleValue;
			}
		}
	}

	void cPdhCounter::GetProcessStatus(double &commitByte)
	{
		PDH_STATUS status; 
		PDH_FMT_COUNTERVALUE counterValue;
		if (!_bInitProcess) return;

		status = PdhGetFormattedCounterValue(_processStruct._pdh_counter_process_privateBytes, PDH_FMT_DOUBLE, NULL, &counterValue);
		if (status == 0) commitByte = counterValue.doubleValue;
	}

	void cPdhCounter::GetMemoryStatus(double &availableByte, double &nonPagedBytes)
	{
		PDH_STATUS status;
		PDH_FMT_COUNTERVALUE counterValue;
		if (!_bInitMemory) return;
		status = PdhGetFormattedCounterValue(_memoryStruct._pdh_counter_memory_availableMemoryMBytes, PDH_FMT_DOUBLE, NULL, &counterValue);
		if (status == 0) availableByte = counterValue.doubleValue;
		status = PdhGetFormattedCounterValue(_memoryStruct._pdh_counter_memory_nonPagedMemoryMBytes, PDH_FMT_DOUBLE, NULL, &counterValue);
		if (status == 0) nonPagedBytes = counterValue.doubleValue / 1048576;
	}

	void cPdhCounter::TerminatePdhCounter(void)
	{
		for(int i = 0; i <df_PDH_ETHERNET_MAX; ++i)
		{
			if(_ethernetStruct[i]._bUsed)
			{
				PdhRemoveCounter(_ethernetStruct[i]._pdh_counter_network_recvBytes);
				PdhRemoveCounter(_ethernetStruct[i]._pdh_counter_network_sendBytes);
			}
		}

		PdhRemoveCounter(_processStruct._pdh_counter_process_privateBytes);
		PdhRemoveCounter(_memoryStruct._pdh_counter_memory_availableMemoryMBytes);
		PdhRemoveCounter(_memoryStruct._pdh_counter_memory_nonPagedMemoryMBytes);
	}

};


//cPdhCounter cEthernetCounter;
//if (!cEthernetCounter.InitPdhCounter()) return 0;
//double recv, send;

//while(1)
//{
//	recv = 0;
//	send = 0;
//	cEthernetCounter.UpdatePdhQueryData();
//	cEthernetCounter.GetNetworkStatus(recv, send);

//	wprintf(L"Recv Bytes: %lf, Send Bytes: %lf\n", recv, send);
//	Sleep(1000);
//};

//cEthernetCounter.TerminatePdhCounter();