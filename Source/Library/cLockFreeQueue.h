#pragma once
//#include "cLockFreeMemoryPool.h"
#include "cMemoryPool.h"

namespace TUNJI_LIBRARY
{
	template <typename T>
	class cLockFreeQueue
	{
	public:
		cLockFreeQueue(): _usedTailIndex(0), _usedHeadIndex(0)
		{
			_memoryPool = new cMemoryPool<st_DATA_NODE, st_DATA_NODE>;
			//_memoryPool = new cMemoryPoolLockFree<st_DATA_NODE, st_DATA_NODE>;
			SetIndexNode();
			_pHead->pDataNode = _memoryPool->Alloc();
			_pTail->pDataNode = _pHead->pDataNode; 

			//_trackCount = 0;
			//memset(&_tracker, 0, sizeof(st_DATA_NODE) * 500);
		}

		~cLockFreeQueue()
		{
			ClearBuffer();

			_memoryPool->Free(_pHead->pDataNode);

			UINT64 *pMemory = (UINT64*)_pHead;
			pMemory -= _usedHeadIndex;
			delete[] pMemory;

			pMemory = (UINT64*)_pTail;
			pMemory -= _usedTailIndex;
			delete[] pMemory;

			delete _memoryPool;
		}

		//	Head Dummy Node�� ����
		UINT64 GetTotalBlockCount(void)
		{
			return _memoryPool->GetTotalBlockCount() - 1;
		}

		UINT64 GetUsedBlockCount(void)
		{
			return _memoryPool->GetUsedBlockCount() - 1;
		}

		void Enqueue(T &data)
		{
			st_INDEX_NODE tail; 
			st_DATA_NODE *pTailNext;

			st_DATA_NODE *pNewNode = _memoryPool->Alloc();
			pNewNode->data = data;
			pNewNode->pNext = nullptr;

			while(1)
			{
				tail.index = _pTail->index;
				tail.pDataNode = _pTail->pDataNode;
				pTailNext = tail.pDataNode->pNext;

				if (pTailNext == nullptr)
				{
					if (InterlockedCompareExchangePointer((PVOID*)&tail.pDataNode->pNext, (void*)pNewNode, (void*)pTailNext) == pTailNext)
					{
						//	tail�� ���� newNode�� �����Ѿ���. 
						InterlockedCompareExchange128((LONG64*)_pTail, tail.index + 1, (LONG64)pNewNode, (LONG64*)&tail);
						break;
					}
				}
				else
				{
					//	tail Next�� ���϶� ���� Enqueue�Ϸ��� �� �� ���� 
					//	InterlockedCompareExchangePointer�� �ٲ��ְ� �� �Ŀ�, 
					//	tail�� �о��ش�. �׷��� �̹� �������� tail�� �о ���� �� ����. 
					//	tail�� next�� null�� �ƴϸ�, �о��ش�.
					InterlockedCompareExchange128((LONG64*)_pTail, tail.index + 1, (LONG64)pTailNext, (LONG64*)&tail);
				}
			}

			//UINT64	index = InterlockedIncrement(&_trackCount) % 500;
			//_tracker[index].pData = data;
			//_tracker[index].type = st_TRACKER::en_DEQ;
			
		}

		bool Dequeue(T &data)
		{
			st_INDEX_NODE head;
			st_INDEX_NODE tail;
			st_DATA_NODE *pNext;

			while (1)
			{
				head.index = _pHead->index;
				head.pDataNode = _pHead->pDataNode;
				pNext = head.pDataNode->pNext;

				tail.index = _pTail->index;
				tail.pDataNode = _pTail->pDataNode;

				if(_pHead->index == head.index)
				{
					if (tail.pDataNode == head.pDataNode)
					{
						//���� ��尡 Tail���� ���ٸ�, Next�� null���� �ƴ��� Ȯ���ϰ�, null�̸� �����Ѵ�.
						if (pNext == nullptr)
						{
							data = 0;
							return false;
						}

						InterlockedCompareExchange128((LONG64*)_pTail, tail.index + 1, (LONG64)pNext, (LONG64*)&tail);
					}
					else
					{
						//	���� ����尡 Free�ǰ� ���� Enqueue�ɶ� nullptr�� next��尡 �ٲ� �� �ֱ� ������ 
						//	��ܿ��� Next��带 �̸� �޾Ƴ��´�. 
						if (pNext == nullptr) continue;
						data = pNext->data;
						if (InterlockedCompareExchange128((LONG64*)_pHead, head.index + 1, (LONG64)pNext, (LONG64*)&head))
						{
							_memoryPool->Free(head.pDataNode);
							break;
						}
					}
				}
			};

			return true;
			//UINT64	index = InterlockedIncrement(&_trackCount) % 500;
			//_tracker[index].pData = data;
			//_tracker[index].type = st_TRACKER::en_DEQ;
		}
		
		void ClearBuffer(void)
		{
			st_DATA_NODE *pHead = _pHead->pDataNode;
			while (pHead->pNext != nullptr)
			{
				st_DATA_NODE *pData = pHead->pNext;
				_memoryPool->Free(pData);
				pHead = pData;
			}
		}

		bool IsEmpty(void)
		{
			return _pHead->pDataNode == _pTail->pDataNode;
		}

	private:
		void SetIndexNode(void)
		{
			//	CAS�� ����ϱ� ���ؼ��� 16��Ʈ�� �������� �޸� �ּҰ� �ʿ���
			//	64��Ʈ ȯ�濡���� �޸� ������ 8����Ʈ�̹Ƿ� 
			//	24����Ʈ��ŭ �Ҵ�޾Ƽ� 8�� �������� ������ ã�� ĳ�����Ѵ�.
			UINT64 *pMemoryHead = new UINT64[3];
			UINT64 *pMemoryTail = new UINT64[3];

			for (int i = 0; i < 2; ++i)
			{
				if ((UINT64*)(((UINT64)&pMemoryHead[i] + 15) & (~15)) == &pMemoryHead[i])
				{
					_pHead = (st_INDEX_NODE*)&pMemoryHead[i];
					_pHead->pDataNode = nullptr;
					_pHead->index = 0;
					_usedHeadIndex = i;
					break;
				}
			}

			for (int i = 0; i < 2; ++i)
			{
				if ((UINT64*)(((UINT64)&pMemoryTail[i] + 15) & (~15)) == &pMemoryTail[i])
				{
					_pTail = (st_INDEX_NODE*)&pMemoryTail[i];
					_pTail->pDataNode = nullptr;
					_pTail->index = 0;
					_usedTailIndex = i;
					break;
				}
			}
		}

		struct st_DATA_NODE
		{
			st_DATA_NODE()
			{
				data = 0; 
				pNext = nullptr;
			}

			T	data;
			st_DATA_NODE *pNext;
		};

		struct st_INDEX_NODE
		{
			st_INDEX_NODE()
			{
				pDataNode = nullptr;
				index = 0;
			}

			st_DATA_NODE *pDataNode; 
			UINT64	index;
		};

		struct st_TRACKER
		{
			enum TRACK_TYPE
			{
				en_ENQ, en_DEQ
			};

			T *pData;
			TRACK_TYPE type;
		};


		st_INDEX_NODE *_pHead;
		st_INDEX_NODE *_pTail;
		
		int _usedHeadIndex; 
		int _usedTailIndex;

		//st_TRACKER		_tracker[500];
		//UINT64			_trackCount;
		cMemoryPool<st_DATA_NODE, st_DATA_NODE> *_memoryPool;
		//cMemoryPoolLockFree<st_DATA_NODE, st_DATA_NODE> *_memoryPool;
	};
};