#pragma once
#include <Windows.h>

namespace TUNJI_LIBRARY
{
	template<class T, class U>
	class cMemoryPool
	{
	public:
		enum
		{
			en_OFFBIT = 706,
		};
		//������
		cMemoryPool() : _iTotalBlock(0), _iUsedBlock(0)
		{
			_pTop = NULL;
			InitializeSRWLock(&_blockSRWL);
		}

		//�ı���
		~cMemoryPool()
		{
			//���� ��ȯ�Ǿ����� ���� �޸𸮱��� �ؼ� ���� ����
			while (_pTop != NULL)
			{
				st_BLOCK_NODE *ptr = _pTop->pNextBlock;
				free(_pTop);
				_pTop = ptr;
			}
		}

		//��� �Ҵ�
		T*		Alloc(void)
		{
			AcquireSRWLockExclusive(&_blockSRWL);
			++_iUsedBlock;
			if (_pTop == NULL)
			{
				st_BLOCK_NODE *ptr;
				ptr = (st_BLOCK_NODE*)malloc(sizeof(st_BLOCK_NODE));
				ptr->dwOffbit = en_OFFBIT;
				ptr->refCount = 1;
				ptr->pNextBlock = nullptr;
				++_iTotalBlock;
				ReleaseSRWLockExclusive(&_blockSRWL);
				return &(ptr->Data);
			}

			T *ptr = &(_pTop->Data);
			_pTop->refCount = 1;
			_pTop->dwOffbit = en_OFFBIT;
			st_BLOCK_NODE * pTemp = _pTop->pNextBlock;
			_pTop = pTemp;
			ReleaseSRWLockExclusive(&_blockSRWL);
			return ptr;
		}

		//Increase reference count 
		int	AddRef(T* data)
		{
			DWORD *ptr = (DWORD*)data;
			st_BLOCK_NODE *node;
			ptr -= 1;

			node = (st_BLOCK_NODE*)ptr;
			return InterlockedIncrement(&node->refCount);
		}

		//��� ��ȯ
		BOOL	Free(T* data)
		{
			DWORD *ptr = (DWORD*)data;
			ptr -= 1;
			st_BLOCK_NODE *pTemp = (st_BLOCK_NODE*)ptr;

			if (pTemp->dwOffbit != (DWORD)en_OFFBIT) return FALSE;
			if (--pTemp->refCount != 0) return FALSE;
			//if (InterlockedDecrement(&pTemp->refCount) > 0) return FALSE;

			AcquireSRWLockExclusive(&_blockSRWL);

			pTemp = _pTop;
			_pTop = (st_BLOCK_NODE*)ptr;
			_pTop->pNextBlock = pTemp;
			_iUsedBlock--;

			ReleaseSRWLockExclusive(&_blockSRWL);
			return TRUE;
		}

		void ClearBuffer(void)
		{
			while (_pTop != NULL)
			{
				st_BLOCK_NODE *ptr = _pTop->pNextBlock;
				free(_pTop);
				_pTop = ptr;
			}
		}

		//�Ҵ���� ����� ���� ���
		int		GetTotalBlockCount(void)
		{
			return _iTotalBlock;
		}

		//���� ���� �ִ� ����� ���� ���
		int		GetUsedBlockCount(void)
		{
			return _iUsedBlock;
		}

	private:
		struct st_BLOCK_NODE
		{
			//st_BLOCK_NODE()
			//{
			//	pNextBlock = NULL;
			//	dwOffbit = (DWORD)en_OFFBIT;
			//}

			DWORD dwOffbit;
			T Data;
			UINT64 refCount;
			st_BLOCK_NODE *pNextBlock;
		};

		st_BLOCK_NODE*		_pTop;
		int					_iTotalBlock;
		int					_iUsedBlock;
		SRWLOCK				_blockSRWL;
	};



	template<class T>
	class cMemoryPool < T, T>
	{
	public:
		enum
		{
			en_OFFBIT = 706,
		};
		//������
		cMemoryPool() : _iTotalBlock(0)
		{
			_pTop = NULL;
			InitializeSRWLock(&_blockSRWL);
		}

		//�ı���
		~cMemoryPool()
		{
			//���� ��ȯ�Ǿ����� ���� �޸𸮱��� �ؼ� ���� ����
			while (_pTop != NULL)
			{
				st_BLOCK_NODE *ptr = _pTop->pNextBlock;
				delete _pTop;
				_pTop = ptr;
			}
		}

		//��� �Ҵ�
		T*		Alloc(void)
		{
			AcquireSRWLockExclusive(&_blockSRWL);
			++_iUsedBlock;

			if (_pTop == NULL)
			{
				st_BLOCK_NODE *ptr;
				ptr = new st_BLOCK_NODE;
				ptr->dwOffbit = en_OFFBIT;
				ptr->refCount = 1;
				++_iTotalBlock;
				ReleaseSRWLockExclusive(&_blockSRWL);
				return &(ptr->Data);
			}
			T *ptr = &(_pTop->Data);
			_pTop->refCount = 1;
			st_BLOCK_NODE * pTemp = _pTop->pNextBlock;
			_pTop = pTemp;

			ReleaseSRWLockExclusive(&_blockSRWL);
			return ptr;
		}

		UINT64	AddRef(T* data)
		{
			DWORD *ptr = (DWORD*)data;
			st_BLOCK_NODE *node;
			ptr -= 2;

			node = (st_BLOCK_NODE*)ptr;
			return InterlockedIncrement(&node->refCount);
		}

		//��� ��ȯ
		BOOL	Free(T* data)
		{
			DWORD *ptr = (DWORD*)data;
			ptr -= 2;
			st_BLOCK_NODE *pTemp = (st_BLOCK_NODE*)ptr;

			if (pTemp->dwOffbit != (DWORD)en_OFFBIT) return FALSE;
			if (--pTemp->refCount > 0) return FALSE;
			//if (InterlockedDecrement(&pTemp->refCount) > 0) return FALSE;

			AcquireSRWLockExclusive(&_blockSRWL);
			pTemp = _pTop;
			_pTop = (st_BLOCK_NODE*)ptr;
			_pTop->pNextBlock = pTemp;
			_iUsedBlock--;
			ReleaseSRWLockExclusive(&_blockSRWL);
			return TRUE;
		}

		void ClearBuffer(void)
		{
			AcquireSRWLockExclusive(&_blockSRWL);
			while (_pTop != NULL)
			{
				st_BLOCK_NODE *ptr = _pTop->pNextBlock;
				delete _pTop;
				_pTop = ptr;
			}
			ReleaseSRWLockExclusive(&_blockSRWL);
		}

		//�Ҵ���� ����� ���� ���
		int		GetTotalBlockCount(void)
		{
			return _iTotalBlock;
		}

		//���� ���� �ִ� ����� ���� ���
		int		GetUsedBlockCount(void)
		{
			return _iUsedBlock;
		}

	private:
		struct st_BLOCK_NODE
		{
			DWORD dwOffbit;
			T Data;
			UINT64 refCount;
			st_BLOCK_NODE *pNextBlock;
		};

		st_BLOCK_NODE*	_pTop;
		int				_iTotalBlock;
		int				_iUsedBlock;
		SRWLOCK			_blockSRWL;
	};
}