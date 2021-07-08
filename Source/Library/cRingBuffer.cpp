// RingBuffer.cpp : 콘솔 응용 프로그램에 대한 진입점을 정의합니다.
//
#include "cRingBuffer.h"
#include <memory.h>

namespace TUNJI_LIBRARY
{
	void cRingBuffer::Initial(int iBufferSize)
	{
		_chpBuff = new char[iBufferSize];
		_iFront = 0;
		_iRear = 0;
		_iBufferSize = iBufferSize;
	}

	cRingBuffer::cRingBuffer()
	{
		Initial(en_DEFAULT_SIZE);
	}

	cRingBuffer::cRingBuffer(int iBufferSize)
	{
		Initial(iBufferSize);
	}

	cRingBuffer::~cRingBuffer()
	{
		delete[] _chpBuff;
	}
	int cRingBuffer::GetTotalSize(void)
	{
		return _iBufferSize;
	}


	int cRingBuffer::GetUsedSize(void)
	{
		int interval = _iRear - _iFront;

		if (interval > 0)
		{
			return interval;
		}
		else if (interval < 0)
		{
			return _iBufferSize + interval;
		}
		else
		{
			return 0;
		}
	}

	//원래 비어있어야하는 위치는 제외하고 return한다. 
	int cRingBuffer::GetEmptySize(void)
	{
		int interval = _iFront - _iRear;

		if (interval < 0)
		{
			return _iBufferSize + interval - 1;
		}
		else if (interval > 0)
		{
			return interval - 1;
		}
		else
		{
			return _iBufferSize - 1;
		}
	}

	int cRingBuffer::GetConstantReadSize(void)
	{
		int front = _iFront;
		int rear = _iRear;
		int interval = rear - front;

		if (interval > 0)
		{
			return interval;
		}
		else if (interval < 0)
		{
			if (front != _iBufferSize - 1)
			{
				return _iBufferSize - front - 1;
			}
			else
			{
				return rear + 1;
			}
		}
		else
			return 0;
	}

	int cRingBuffer::GetConstantWriteSize(void)
	{
		int rear = _iRear;
		int front = _iFront;
		int interval = front - rear;

		if (interval > 0)
		{
			return interval - 1;
		}
		else
		{
			if (rear != _iBufferSize - 1)
			{
				return _iBufferSize - rear - 1;
			}
			else
			{
				return front;
			}
		}
	}

	int cRingBuffer::Enqueue(char *chp, int iSize)
	{
		int rear = _iRear;
		int front = _iFront;
		int bufSize = _iBufferSize;
		int iRemainSize, iConstSize;

		if ((iRemainSize = GetEmptySize()) == 0) return 0;
		iConstSize = GetConstantWriteSize();
		

		char *ptr = &_chpBuff[(rear + 1) % bufSize];

		if (iConstSize < iSize)
		{
			if (iRemainSize < iSize)
				iSize = iRemainSize;

			memcpy(ptr, chp, iConstSize);

			ptr = _chpBuff;
			memcpy(ptr, chp + iConstSize, iSize - iConstSize);
			_iRear = (rear + iSize) % bufSize;

			return iSize;
		}

		memcpy(ptr, chp, iSize);
		_iRear = (rear + iSize) % bufSize;

		return iSize;
	}

	int cRingBuffer::Dequeue(char *chp, int iSize)
	{
		int front = _iFront;
		int rear = _iRear;
		int bufSize = _iBufferSize;
		int iUsedSize, iConstSize;

		if ((iUsedSize = GetUsedSize()) == 0) return 0;
		iConstSize = GetConstantReadSize();


		char*ptr = &_chpBuff[(front + 1) % bufSize];

		if (iConstSize < iSize)
		{
			if (iSize > iUsedSize)
				iSize = iUsedSize;
			memcpy(chp, ptr, iConstSize);

			ptr = _chpBuff;
			memcpy(chp + iConstSize, ptr, iSize - iConstSize);
			_iFront = (front + iSize) % bufSize;

			return iSize;
		}
		memcpy(chp, ptr, iSize);
		_iFront = (front + iSize) % bufSize;

		return iSize;
	}

	int cRingBuffer::Peek(char *chp, int iSize)
	{
		int front = _iFront;
		int rear = _iRear;
		int bufSize = _iBufferSize;
		int iUsedSize, iConstSize;

		if ((iUsedSize = GetUsedSize()) == 0) return 0;
		iConstSize = GetConstantReadSize();

		char * ptr = &_chpBuff[(front + 1) % _iBufferSize];

		if (iConstSize < iSize)
		{
			if (iSize > iUsedSize)
				iSize = iUsedSize;
			memcpy(chp, ptr, iConstSize);

			ptr = _chpBuff;
			memcpy(chp + iConstSize, ptr, iSize - iConstSize);

			return iSize;
		}
		memcpy(chp, ptr, iSize);

		return iSize;
	}

	int cRingBuffer::MoveRearforSave(int iSize)
	{
		_iRear = (_iRear + iSize) % _iBufferSize;
		return _iRear;
	}

	void cRingBuffer::MoveFrontforDelete(int iSize)
	{
		_iFront = (_iFront + iSize) % _iBufferSize;
	}

	void cRingBuffer::ClearBuffer(void)
	{
		_iFront = 0;
		_iRear = 0;	
	}

	char* cRingBuffer::GetStartptr(void)
	{
		return _chpBuff;
	}

	char* cRingBuffer::GetWriteptr(void)
	{
		return &_chpBuff[(_iRear + 1) % _iBufferSize];
	}

	char* cRingBuffer::GetReadptr(void)
	{
		return &_chpBuff[(_iFront + 1) % _iBufferSize];
	}
	
	int cRingBuffer::GetFrontPos(void)
	{
		return _iFront;
	}
}