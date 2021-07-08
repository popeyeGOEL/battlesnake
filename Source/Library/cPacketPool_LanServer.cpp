#include "cMemoryPoolTLS.h"
#include "cPacketPool_LanServer.h"

namespace TUNJI_LIBRARY
{
	cMemoryPoolTLS<cLanPacketPool>* cLanPacketPool::_packetMemoryPool = 0;

	cLanPacketPool::cLanPacketPool()
	{
		_chpBuff = new char[en_BUFSIZE_DEFALUT];
		memset(_chpBuff, 0, en_BUFSIZE_DEFALUT);
		_iBufSize = en_BUFSIZE_DEFALUT - en_HEADERSIZE_DEFAULT;
		_iUsedSize = 0;
		_iRear = en_HEADERSIZE_DEFAULT - 1;
		_iFront = en_HEADERSIZE_DEFAULT - 1;
		_pHeader = (st_LAN_HEADER*)(_chpBuff + en_HEADERSIZE_DEFAULT - sizeof(st_LAN_HEADER));
		_iHeaderSize = sizeof(st_LAN_HEADER);
		_errorCode = en_NONE;
	}

	cLanPacketPool::cLanPacketPool(int bufSize)
	{
		_chpBuff = new char[bufSize];
		memset(_chpBuff, 0, bufSize);
		_iBufSize = bufSize - en_HEADERSIZE_DEFAULT;
		_iUsedSize = 0;
		_iRear = en_HEADERSIZE_DEFAULT - 1;
		_iFront = en_HEADERSIZE_DEFAULT - 1;
		_pHeader = (st_LAN_HEADER*)(_chpBuff + en_HEADERSIZE_DEFAULT - sizeof(st_LAN_HEADER));
		_iHeaderSize = sizeof(st_LAN_HEADER);
		_errorCode = en_NONE;
	}

	cLanPacketPool::cLanPacketPool(cLanPacketPool &cSrcPacket)
	{
		_chpBuff = new char[cSrcPacket.GetBufSize()];
		memcpy(_chpBuff, cSrcPacket.GetBufptr(), cSrcPacket.GetBufSize());
		_iUsedSize = cSrcPacket._iUsedSize;
		_iRear = cSrcPacket._iRear;
		_iFront = cSrcPacket._iFront;
		_pHeader = (st_LAN_HEADER*)(_chpBuff + en_HEADERSIZE_DEFAULT - sizeof(st_LAN_HEADER));
		_iHeaderSize = sizeof(st_LAN_HEADER);
		_errorCode = en_NONE;

	}

	cLanPacketPool& cLanPacketPool::operator=(cLanPacketPool &cSrcPacket)
	{
		//복사 생성자
		_chpBuff = new char[cSrcPacket.GetBufSize()];
		memcpy(_chpBuff, cSrcPacket.GetBufptr(), cSrcPacket.GetBufSize());
		_iUsedSize = cSrcPacket._iUsedSize;
		_iRear = cSrcPacket._iRear;
		_iFront = cSrcPacket._iFront;
		_pHeader = (st_LAN_HEADER*)(_chpBuff + en_HEADERSIZE_DEFAULT - sizeof(st_LAN_HEADER));
		_iHeaderSize = sizeof(st_LAN_HEADER);
		_errorCode = en_NONE;

		return *this;
	}

	cLanPacketPool::~cLanPacketPool()
	{
		if (_iBufSize != 0)
			delete[] _chpBuff;
	}

	cLanPacketPool& cLanPacketPool::operator<<(BYTE byValue)
	{
		Serialize((char*)&byValue, sizeof(byValue));
		return *this;
	}
	cLanPacketPool& cLanPacketPool::operator<<(WORD wValue)
	{
		Serialize((char*)&wValue, sizeof(wValue));
		return *this;
	}
	cLanPacketPool& cLanPacketPool::operator<<(DWORD dwValue)
	{
		Serialize((char*)&dwValue, sizeof(dwValue));
		return *this;
	}
	cLanPacketPool&	cLanPacketPool::operator<<(WCHAR chValue)
	{
		Serialize((char*)&chValue, sizeof(chValue));
		return *this;
	}

	cLanPacketPool&	cLanPacketPool::operator<<(UINT64 uiValue)
	{
		Serialize((char*)&uiValue, sizeof(uiValue));
		return *this;
	}

	cLanPacketPool& cLanPacketPool::operator<<(char chValue)
	{
		Serialize((char*)&chValue, sizeof(chValue));
		return *this;
	}
	cLanPacketPool& cLanPacketPool::operator<<(int iValue)
	{
		Serialize((char*)&iValue, sizeof(iValue));
		return *this;
	}
	cLanPacketPool& cLanPacketPool::operator<<(unsigned int uiValue)
	{
		Serialize((char*)&uiValue, sizeof(uiValue));
		return *this;
	}
	cLanPacketPool& cLanPacketPool::operator<<(long lValue)
	{
		Serialize((char*)&lValue, sizeof(lValue));
		return *this;
	}
	cLanPacketPool& cLanPacketPool::operator<<(long long dlValue)
	{
		Serialize((char*)&dlValue, sizeof(dlValue));
		return *this;
	}

	cLanPacketPool& cLanPacketPool::operator<<(float fValue)
	{
		Serialize((char*)&fValue, sizeof(fValue));
		return *this;
	}
	cLanPacketPool& cLanPacketPool::operator<<(double dValue)
	{
		Serialize((char*)&dValue, sizeof(dValue));
		return *this;
	}

	cLanPacketPool& cLanPacketPool::operator<<(long double ldValue)
	{
		Serialize((char*)&ldValue, sizeof(ldValue));
		return *this;
	}


	cLanPacketPool& cLanPacketPool::operator >> (BYTE &byValue)
	{
		Deserialize((char*)&byValue, sizeof(BYTE));
		return *this;
	}
	cLanPacketPool& cLanPacketPool::operator >> (WORD &wValue)
	{
		Deserialize((char*)&wValue, sizeof(WORD));
		return *this;
	}
	cLanPacketPool& cLanPacketPool::operator >> (DWORD &dwValue)
	{
		Deserialize((char*)&dwValue, sizeof(DWORD));
		return *this;
	}
	cLanPacketPool&	cLanPacketPool::operator >> (WCHAR &chValue)
	{
		Deserialize((char*)&chValue, sizeof(WCHAR));
		return *this;
	}

	cLanPacketPool&	cLanPacketPool::operator >> (UINT64 &uiValue)
	{
		Deserialize((char*)&uiValue, sizeof(UINT64));
		return *this;
	}

	cLanPacketPool& cLanPacketPool::operator >> (char &chValue)
	{
		Deserialize((char*)&chValue, sizeof(char));
		return *this;
	}
	cLanPacketPool& cLanPacketPool::operator >> (int &iValue)
	{
		Deserialize((char*)&iValue, sizeof(int));
		return *this;
	}
	cLanPacketPool& cLanPacketPool::operator >> (unsigned int &uiValue)
	{
		Deserialize((char*)&uiValue, sizeof(unsigned int));
		return *this;
	}
	cLanPacketPool& cLanPacketPool::operator >> (long &lValue)
	{
		Deserialize((char*)&lValue, sizeof(long));
		return *this;
	}

	cLanPacketPool& cLanPacketPool::operator >> (long long &dlValue)
	{
		Deserialize((char*)&dlValue, sizeof(long long));
		return *this;
	}

	cLanPacketPool& cLanPacketPool::operator >> (float &fValue)
	{
		Deserialize((char*)&fValue, sizeof(float));
		return *this;
	}
	cLanPacketPool& cLanPacketPool::operator >> (double &dValue)
	{
		Deserialize((char*)&dValue, sizeof(double));
		return *this;
	}

	cLanPacketPool& cLanPacketPool::operator >> (long double &ldValue)
	{
		Deserialize((char*)&ldValue, sizeof(long double));
		return *this;
	}
	void cLanPacketPool::clear(void)
	{
		_iFront = en_HEADERSIZE_DEFAULT - 1;
		_iRear = en_HEADERSIZE_DEFAULT - 1;
		_iUsedSize = 0;
		_errorCode = en_NONE;
	}

	int cLanPacketPool::GetBufSize(void)
	{
		return _iBufSize + _iHeaderSize;
	}

	int cLanPacketPool::GetDataSize(void)
	{
		return _iUsedSize;
	}

	int	cLanPacketPool::GetDataSizewithHeader(void)
	{
		return _iUsedSize + +_iHeaderSize;
	}

	char * cLanPacketPool::GetBufptr(void)
	{
		return _chpBuff;
	}

	char* cLanPacketPool::GetReadptr(void)
	{
		return (_chpBuff + _iFront + 1);
	}

	char* cLanPacketPool::GetWriteptr(void)
	{
		return(_chpBuff + _iRear + 1);
	}

	int cLanPacketPool::MoveFrontforDelete(int iSize)
	{
		_iFront = _iFront + iSize;
		_iUsedSize -= iSize;
		return _iFront;
	}

	int cLanPacketPool::MoveRearforSave(int iSize)
	{
		_iRear = _iRear + iSize;
		_iUsedSize += iSize;
		return _iRear;
	}

	void cLanPacketPool::Custom_SetLanHeader(WORD payload)
	{
		_pHeader->payload = payload;
	}

	int cLanPacketPool::Serialize(char *chpSrc, int iSrcSize)
	{
		//데이터 저장
		int used = _iUsedSize;
		int rear = _iRear;
		int bufSize = _iBufSize;

		char *ptr = (_chpBuff + rear + 1);
		if (iSrcSize >= bufSize - used - 1)
			iSrcSize = bufSize - used - 1;
		memcpy(ptr, chpSrc, iSrcSize);
		_iRear = rear + iSrcSize;
		_iUsedSize = used + iSrcSize;
		return iSrcSize;
	}

	int cLanPacketPool::Deserialize(char *chpDest, int iSize)
	{
		//데이터 출력
		int front = _iFront;
		int used = _iUsedSize;

		if (iSize > used)
		{
			/*packetException err;
			wsprintf(err.errLog, L"Size Error");
			_errorCode = en_DESERIALIZE_ERROR;
			throw err;*/
			return 0;
		}

		char *ptr = (_chpBuff + front + 1);
		memcpy(chpDest, ptr, iSize);
		_iFront = front + iSize;
		_iUsedSize = used - iSize;
		return iSize;
	}

	cLanPacketPool* cLanPacketPool::Alloc(void)
	{
		cLanPacketPool *packet = _packetMemoryPool->Alloc();
		packet->clear();
		return packet;
	}

	void cLanPacketPool::AddRef(cLanPacketPool* pPacket)
	{
		_packetMemoryPool->AddRef(pPacket);
	}

	void cLanPacketPool::Free(cLanPacketPool* pPacket)
	{
		_packetMemoryPool->Free(pPacket);
	}

	int cLanPacketPool::GetUsedPoolSize(void)
	{
		return _packetMemoryPool->GetUsedBlockCount();
	}

	int cLanPacketPool::GetTolalPoolSize(void)
	{
		return _packetMemoryPool->GetTotalBlockCount();
	}

	bool cLanPacketPool::ConstructChunkPool(void)
	{
		if (_packetMemoryPool != nullptr) return false;
		_packetMemoryPool = new cMemoryPoolTLS < cLanPacketPool>;
		return _packetMemoryPool->SetTlsIndex();
	}

	void cLanPacketPool::DestructChunkPool(void)
	{
		if (_packetMemoryPool == nullptr) return;

		if (_packetMemoryPool->FreeTlsIndex())
		{
			delete _packetMemoryPool;
			_packetMemoryPool = nullptr;
		}
	}

	void cLanPacketPool::AllocPacketChunk(void)
	{
		return _packetMemoryPool->AllocChunk();
	}

	void cLanPacketPool::FreePacketChunk(void)
	{
		cMemoryPoolTLS<cLanPacketPool>::cMemoryChunk *pChunk = _packetMemoryPool->GetChunkPtr();
		if(pChunk != nullptr)
		{
			_packetMemoryPool->FreeChunk(pChunk);
		}
	}
};