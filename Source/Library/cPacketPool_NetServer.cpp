#include "cMemoryPoolTLS.h"
#include "cPacketPool_NetServer.h"

namespace TUNJI_LIBRARY
{
	cMemoryPoolTLS<cNetPacketPool>* cNetPacketPool::_packetMemoryPool = 0;
	cNetPacketPool::cNetPacketPool()
	{
		_chpBuff = new char[en_BUFSIZE_DEFALUT];
		memset(_chpBuff, 0, en_BUFSIZE_DEFALUT);
		_iBufSize = en_BUFSIZE_DEFALUT - en_HEADERSIZE_DEFAULT;
		_iUsedSize = 0;
		_iRear = en_HEADERSIZE_DEFAULT - 1;
		_iFront = en_HEADERSIZE_DEFAULT - 1;
		_pHeader = (st_NET_HEADER*)_chpBuff;

		_bEncoded = false;
		_errorCode = en_NONE;
	}

	cNetPacketPool::cNetPacketPool(int bufSize)
	{
		_chpBuff = new char[bufSize];
		memset(_chpBuff, 0, bufSize);
		_iBufSize = bufSize - en_HEADERSIZE_DEFAULT;
		_iUsedSize = 0;
		_iRear = en_HEADERSIZE_DEFAULT - 1;
		_iFront = en_HEADERSIZE_DEFAULT - 1;
		_pHeader = (st_NET_HEADER*)_chpBuff;

		_bEncoded = false;
		_errorCode = en_NONE;
	}

	cNetPacketPool::cNetPacketPool(cNetPacketPool &cSrcPacket)
	{
		_chpBuff = new char[cSrcPacket.GetBufSize()];
		memcpy(_chpBuff, cSrcPacket.GetBufptr(), cSrcPacket.GetBufSize());
		_iUsedSize = cSrcPacket._iUsedSize;
		_iRear = cSrcPacket._iRear;
		_iFront = cSrcPacket._iFront;
		_pHeader = (st_NET_HEADER*)_chpBuff;

		_bEncoded = false;
		_errorCode = en_NONE;
	}

	cNetPacketPool& cNetPacketPool::operator=(cNetPacketPool &cSrcPacket)
	{
		//복사 생성자
		_chpBuff = new char[cSrcPacket.GetBufSize()];
		memcpy(_chpBuff, cSrcPacket.GetBufptr(), cSrcPacket.GetBufSize());
		_iUsedSize = cSrcPacket._iUsedSize;
		_iRear = cSrcPacket._iRear;
		_iFront = cSrcPacket._iFront;
		_pHeader = (st_NET_HEADER*)_chpBuff;


		_bEncoded = false;
		_errorCode = en_NONE;

		return *this;
	}

	cNetPacketPool::~cNetPacketPool()
	{
		if (_iBufSize != 0)
			delete[] _chpBuff;
	}

	cNetPacketPool& cNetPacketPool::operator<<(BYTE byValue)
	{
		Serialize((char*)&byValue, sizeof(byValue));
		return *this;
	}
	cNetPacketPool& cNetPacketPool::operator<<(WORD wValue)
	{
		Serialize((char*)&wValue, sizeof(wValue));
		return *this;
	}
	cNetPacketPool& cNetPacketPool::operator<<(DWORD dwValue)
	{
		Serialize((char*)&dwValue, sizeof(dwValue));
		return *this;
	}
	cNetPacketPool&	cNetPacketPool::operator<<(WCHAR chValue)
	{
		Serialize((char*)&chValue, sizeof(chValue));
		return *this;
	}

	cNetPacketPool&	cNetPacketPool::operator<<(UINT64 uiValue)
	{
		Serialize((char*)&uiValue, sizeof(uiValue));
		return *this;
	}

	cNetPacketPool& cNetPacketPool::operator<<(char chValue)
	{
		Serialize((char*)&chValue, sizeof(chValue));
		return *this;
	}
	cNetPacketPool& cNetPacketPool::operator<<(int iValue)
	{
		Serialize((char*)&iValue, sizeof(iValue));
		return *this;
	}
	cNetPacketPool& cNetPacketPool::operator<<(unsigned int uiValue)
	{
		Serialize((char*)&uiValue, sizeof(uiValue));
		return *this;
	}
	cNetPacketPool& cNetPacketPool::operator<<(long lValue)
	{
		Serialize((char*)&lValue, sizeof(lValue));
		return *this;
	}
	cNetPacketPool& cNetPacketPool::operator<<(long long dlValue)
	{
		Serialize((char*)&dlValue, sizeof(dlValue));
		return *this;
	}

	cNetPacketPool& cNetPacketPool::operator<<(float fValue)
	{
		Serialize((char*)&fValue, sizeof(fValue));
		return *this;
	}
	cNetPacketPool& cNetPacketPool::operator<<(double dValue)
	{
		Serialize((char*)&dValue, sizeof(dValue));
		return *this;
	}

	cNetPacketPool& cNetPacketPool::operator<<(long double ldValue)
	{
		Serialize((char*)&ldValue, sizeof(ldValue));
		return *this;
	}

	cNetPacketPool& cNetPacketPool::operator >> (BYTE &byValue)
	{
		Deserialize((char*)&byValue, sizeof(BYTE));
		return *this;
	}
	cNetPacketPool& cNetPacketPool::operator >> (WORD &wValue)
	{
		Deserialize((char*)&wValue, sizeof(WORD));
		return *this;
	}
	cNetPacketPool& cNetPacketPool::operator >> (DWORD &dwValue)
	{
		Deserialize((char*)&dwValue, sizeof(DWORD));
		return *this;
	}
	cNetPacketPool&	cNetPacketPool::operator >> (WCHAR &chValue)
	{
		Deserialize((char*)&chValue, sizeof(WCHAR));
		return *this;
	}

	cNetPacketPool&	cNetPacketPool::operator >> (UINT64 &uiValue)
	{
		Deserialize((char*)&uiValue, sizeof(UINT64));
		return *this;
	}

	cNetPacketPool& cNetPacketPool::operator >> (char &chValue)
	{
		Deserialize((char*)&chValue, sizeof(char));
		return *this;
	}
	cNetPacketPool& cNetPacketPool::operator >> (int &iValue)
	{
		Deserialize((char*)&iValue, sizeof(int));
		return *this;
	}
	cNetPacketPool& cNetPacketPool::operator >> (unsigned int &uiValue)
	{
		Deserialize((char*)&uiValue, sizeof(unsigned int));
		return *this;
	}
	cNetPacketPool& cNetPacketPool::operator >> (long &lValue)
	{
		Deserialize((char*)&lValue, sizeof(long));
		return *this;
	}

	cNetPacketPool& cNetPacketPool::operator >> (long long &dlValue)
	{
		Deserialize((char*)&dlValue, sizeof(long long));
		return *this;
	}

	cNetPacketPool& cNetPacketPool::operator >> (float &fValue)
	{
		Deserialize((char*)&fValue, sizeof(float));
		return *this;
	}
	cNetPacketPool& cNetPacketPool::operator >> (double &dValue)
	{
		Deserialize((char*)&dValue, sizeof(double));
		return *this;
	}

	cNetPacketPool& cNetPacketPool::operator >> (long double &ldValue)
	{
		Deserialize((char*)&ldValue, sizeof(long double));
		return *this;
	}
	void cNetPacketPool::clear(void)
	{
		_iFront = en_HEADERSIZE_DEFAULT - 1;
		_iRear = en_HEADERSIZE_DEFAULT - 1;
		_iUsedSize = 0;
		_bEncoded = false;
		_errorCode = en_NONE;
	}

	int cNetPacketPool::GetBufSize(void)
	{
		return _iBufSize + en_HEADERSIZE_DEFAULT;
	}

	int cNetPacketPool::GetDataSize(void)
	{
		return _iUsedSize;
	}

	int	cNetPacketPool::GetDataSizewithHeader(void)
	{
		return _iUsedSize + en_HEADERSIZE_DEFAULT;
	}

	char * cNetPacketPool::GetBufptr(void)
	{
		return _chpBuff;
	}

	char* cNetPacketPool::GetReadptr(void)
	{
		return (_chpBuff + _iFront + 1);
	}

	char* cNetPacketPool::GetWriteptr(void)
	{
		return(_chpBuff + _iRear + 1);
	}

	int cNetPacketPool::MoveFrontforDelete(int iSize)
	{
		_iFront = _iFront + iSize;
		_iUsedSize -= iSize;
		return _iFront;
	}

	int cNetPacketPool::MoveRearforSave(int iSize)
	{
		_iRear = _iRear + iSize;
		_iUsedSize += iSize;
		return _iRear;
	}

	void cNetPacketPool::Custom_SetNetHeader(st_NET_HEADER header)
	{
		memcpy(_pHeader, &header, sizeof(st_NET_HEADER));
	}

	int cNetPacketPool::Serialize(char *chpSrc, int iSrcSize)
	{
		//데이터 저장
		int used = _iUsedSize;
		int rear = _iRear;
		int bufSize = _iBufSize;

		char *ptr = (_chpBuff + rear + 1);
		if (iSrcSize >= bufSize - used - 1)
		{
			iSrcSize = bufSize - used - 1;
		}
		memcpy(ptr, chpSrc, iSrcSize);
		_iRear = rear + iSrcSize;
		_iUsedSize = used + iSrcSize;
		return iSrcSize;
	}

	int cNetPacketPool::Deserialize(char *chpDest, int iSize)
	{
		//데이터 출력
		int front = _iFront;
		int used = _iUsedSize;

		if (iSize > used)
		{
			packetException err;
			wsprintf(err.errLog, L"Size Error");
			_errorCode = en_DESERIALIZE_ERROR;
			throw err;
		}

		char *ptr = (_chpBuff + front + 1);
		memcpy(chpDest, ptr, iSize);
		_iFront = front + iSize;
		_iUsedSize = used - iSize;
		return iSize;
	}

	void cNetPacketPool::Encode(BYTE packetCode, BYTE XORCode1, BYTE XORCode2)
	{
		//	보내는 암호화 과정 
		//	1. Rand XOR Code 생성
		//	2. Payload 의 checksum 계산
		//	3. Rand XOR Code 로[CheckSum, Payload] 바이트 단위 xor
		//	4. 고정 XOR Code 1 로[Rand XOR Code, CheckSum, Payload] 를 XOR
		//	5. 고정 XOR Code 2 로[Rand XOR Code, CheckSum, Payload] 를 XOR

		if (_bEncoded == true) return;

		st_NET_HEADER *pHeader = _pHeader;
		BYTE checkSum = 0;
		BYTE XORCode = XORCode1 ^ XORCode2;
		WORD dataLen = GetDataSize();
		unsigned char *pRead = (unsigned char*)(_chpBuff + _iFront + 1);

		pHeader->checkCode = packetCode;
		pHeader->randomCode = rand() % 128;
		pHeader->len = dataLen;

		unsigned char *pByte = pRead;
		for (int i = 0; i < dataLen; ++i)
		{
			checkSum += pByte[i];
		}

		pHeader->checkSum = checkSum;

		pByte = pRead - 1;
		for (int i = 0; i < dataLen + 1; ++i)
		{
			pByte[i] = pByte[i] ^ pHeader->randomCode ^ XORCode;
		}

		//pByte = pRead - 2;
		//for (int i = 0; i < dataLen + 2; ++i)
		//{
		//	pByte[i] = pByte[i] ^ XORCode;
		//}

		pByte = pRead - 2;
		pByte[0] = pByte[0] ^ XORCode;

		_bEncoded = true;
	}
	void cNetPacketPool::Decode(BYTE packetCode, BYTE XORCode1, BYTE XORCode2)
	{
		/*	= 받기 복호화 과정
			1. 고정 XOR Code 2 로[Rand XOR Code, CheckSum, Payload] 를 XOR
			2. 고정 XOR Code 1 로[Rand XOR Code, CheckSum, Payload] 를 XOR
			3. Rand XOR Code 를 파악.
			4. Rand XOR Code 로[CheckSum - Payload] 바이트 단위 xor
			5. Payload 를 checksum 공식으로 계산 후 패킷의 checksum 과 비교*/

		st_NET_HEADER *pHeader = _pHeader;

		if (pHeader->checkCode != packetCode)
		{
			packetException err;
			wsprintf(err.errLog, L"Decode Error");
			_errorCode = en_DECODE_ERROR;
			throw err;
		}

		BYTE checkSum = 0;
		BYTE XORCode = XORCode1 ^ XORCode2;
		WORD dataLen;
		BYTE randCode;

		unsigned char *pRead = (unsigned char*)(_chpBuff + _iFront + 1);

		unsigned char *pByte = pRead - 2;
		pByte[0] = pByte[0] ^ XORCode;

		dataLen = pHeader->len;
		randCode = pHeader->randomCode;

		pByte = pRead - 1;
		for (int i = 0; i < dataLen + 1; ++i)
		{
			pByte[i] = pByte[i] ^ randCode ^ XORCode;
		}

		pByte = pRead;
		for (int i = 0; i < dataLen; ++i)
		{
			checkSum += pByte[i];
		}

		if (checkSum != pHeader->checkSum)
		{
			packetException err;
			wsprintf(err.errLog, L"CheckSum Error");
			_errorCode = en_DECODE_ERROR;
			throw err;
		}
	}

	cNetPacketPool* cNetPacketPool::Alloc(void)
	{
		cNetPacketPool *packet = _packetMemoryPool->Alloc();
		packet->clear();
		return packet;
	}

	void cNetPacketPool::AddRef(cNetPacketPool* pPacket)
	{
		_packetMemoryPool->AddRef(pPacket);
	}

	void cNetPacketPool::Free(cNetPacketPool* pPacket)
	{
		_packetMemoryPool->Free(pPacket);
	}

	int cNetPacketPool::GetUsedPoolSize(void)
	{
		return _packetMemoryPool->GetUsedBlockCount();
	}

	int cNetPacketPool::GetTolalPoolSize(void)
	{
		return _packetMemoryPool->GetTotalBlockCount();
	}

	bool cNetPacketPool::ConstructChunkPool(void)
	{
		if (_packetMemoryPool != nullptr) return false;
		_packetMemoryPool = new cMemoryPoolTLS <cNetPacketPool>;
		return _packetMemoryPool->SetTlsIndex();
	}

	void cNetPacketPool::DestructChunkPool(void)
	{
		if (_packetMemoryPool->FreeTlsIndex())
		{
			delete _packetMemoryPool;
			_packetMemoryPool = nullptr;
		}
	}

	void cNetPacketPool::AllocPacketChunk(void)
	{
		return _packetMemoryPool->AllocChunk();
	}

	void cNetPacketPool::FreePacketChunk(void)
	{
		cMemoryPoolTLS<cNetPacketPool>::cMemoryChunk *pChunk = _packetMemoryPool->GetChunkPtr();
		if(pChunk != nullptr)
		{
			_packetMemoryPool->FreeChunk(pChunk);
		}
	}
};