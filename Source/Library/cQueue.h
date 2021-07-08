#pragma once
namespace TUNJI_LIBRARY
{
	template<typename T>
	class cQueue {
	public:
		enum
		{
			DEFAULT_SIZE = 1000
		};

		cQueue(int max = DEFAULT_SIZE) :_iFront(0), _iRear(0), _iTotalSize(max)
		{
			_queue = new T[max];
		}

		bool Enqueue(T &data)
		{
			int iRear = _iRear;
			int iTotalSize = _iTotalSize;

			if ((iRear + 1) % iTotalSize == _iFront) return false;

			iRear = (iRear + 1) % iTotalSize;
			_queue[iRear] = data;

			_iRear = iRear;

			return true;
		}

		void Dequeue(T &data)
		{
			int iFront = _iFront;

			if (iFront == _iRear)
			{
				data = 0;
				return;
			}

			iFront = (iFront + 1) % _iTotalSize;
			data = _queue[iFront];

			_iFront = iFront;
		}

		void Peek(T &data)
		{
			int iFront = _iFront;

			if (iFront == _iRear)
			{
				data = 0;
				return;
			}

			iFront = (iFront + 1) % _iTotalSize;
			data = _queue[iFront];
		}

		bool Peek(T &data, int pos)
		{
			int iFront = _iFront;
			int iRear = _iRear;

			int interval;

			if(iRear > iFront)
			{
				interval = iRear - iFront;
				if (interval <= pos) return false;
			}
			else if(iRear < iFront)
			{
				interval = _iTotalSize - (iFront - iRear);
				if (interval <= pos) return false;
			}
			else if (iFront == iRear)
			{
				data = 0;
				return false;
			}

			iFront = (iFront + pos + 1) % _iTotalSize;
			data = _queue[iFront];
			return true;
		}

		void MoveFrontForDelete(int value)
		{
			_iFront = (_iFront + value) % _iTotalSize;
		}

		void MoveRearForSave(int value)
		{
			_iRear = (_iRear + value) % _iTotalSize;
		}

		const bool isEmpty(void)
		{
			return _iFront == _iRear;
		}

		const bool isFull(void)
		{
			return _iFront == (_iRear + 1) % _iTotalSize;
		}

		const void GetFrontData(T &Data)
		{
			if(_iFront == _iRear)
			{
				Data = 0;
				return;
			}

			Data = _queue[(_iFront + 1) % _iTotalSize];
		}

		const void GetLastData(T &Data)
		{
			if(_iFront == _iRear)
			{
				Data = 0;
				return;
			}
			Data = _queue[_iRear];
		}

		void Clear(void)
		{
			_iFront = _iRear;
		}

		~cQueue()
		{
			delete[] _queue;
		}
	private:
		T *_queue;
		int _iFront;
		int _iRear;
		int _iTotalSize;
	};
}