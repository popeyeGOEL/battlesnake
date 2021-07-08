#pragma once
#include "cMemoryPool.h"
//#include "cMemoryPool_Log.h"
//#include "cLockFreeMemoryPool.h"
#include <list>
#include "cCrashDump.h"

// Chunk 별로 AddRef, Free, Alloc 디버깅 하기
namespace TUNJI_LIBRARY
{
	template<typename T>
	class cMemoryPoolTLS
	{
	public:
		enum
		{
			en_OffBit = 0xacce0706
		};

		class cMemoryChunk
		{
		public:
			enum en_TYPE_REF
			{
				INC_REF = 0, DEC_REF, ALLOC, FREE
			};

			//typedef struct st_REF_TRACKER
			//{
			//	st_REF_TRACKER()
			//	{
			//		type = ALLOC;
			//		freeableCount = 0;
			//		allocableCount = 0;
			//		pChunk = nullptr;
			//	}

			//	en_TYPE_REF type;
			//	UINT64	allocableCount;
			//	UINT64	freeableCount;
			//	cMemoryChunk* pChunk;
			//}st_REF_TRACKER;

			// Chunk안의 데이터 정보
			typedef struct st_CHUNK_BLOCK
			{
				st_CHUNK_BLOCK()
				{
					checkCode = en_OffBit;
				}
				cMemoryChunk *pChunk;
				__int64 checkCode;
				T	data;
			}st_CHUNK_BLOCK;

			enum
			{
				en_MAX_COUNT = 200
			};

			cMemoryChunk() {}
			~cMemoryChunk() {}

			T* AllocBlock(void)
			{
				T* data = &_dataArray[en_MAX_COUNT - _allocableCount].data;
				--_allocableCount;

				return data;
			}

			void Init(void)
			{
				_allocableCount = en_MAX_COUNT;
				_freeableCount = en_MAX_COUNT;
				for (int i = 0; i < _allocableCount; ++i)
				{
					_dataArray[i].pChunk = this;
				}
			}

			int GetAllocableCount(void)
			{
				return _allocableCount;
			}

			int GetFreeableCount(void)
			{
				return _freeableCount;
			}

			bool FreeBlock(void)
			{
				if (InterlockedDecrement(&_freeableCount) == 0)
					return false; 

				return true;
			}

			void AddBlockRef(void)
			{
				UINT64 freeableCount = InterlockedIncrement(&_freeableCount);
			}

		private:
			st_CHUNK_BLOCK _dataArray[en_MAX_COUNT];
			int _allocableCount;
			UINT64 _freeableCount;
			/*st_REF_TRACKER	_tracker[200];
			UINT64			_trackerCount;*/
		};

		cMemoryPoolTLS()
		{
			_tlsIndex = 0;
			_blockAllocCount = 0;
			_memoryPool = new cMemoryPool<cMemoryChunk, cMemoryChunk>;
		}

		~cMemoryPoolTLS() 
		{
			delete _memoryPool;
		}

		T* Alloc(void)
		{
			cMemoryChunk *pChunk = (cMemoryChunk*)TlsGetValue(_tlsIndex);
			InterlockedIncrement(&_blockAllocCount);
			T* data = pChunk->AllocBlock();
			if (pChunk->GetAllocableCount() == 0)
			{
				AllocChunk();
			}
			return data;
		}

		void Free(T* data)
		{
			DWORD *ptr = (DWORD*)data;
			ptr -= 4;
			cMemoryChunk::st_CHUNK_BLOCK * pBlock = (cMemoryChunk::st_CHUNK_BLOCK*)ptr;

			if (pBlock->checkCode != en_OffBit)
			{
				return;
			}

			InterlockedDecrement(&_blockAllocCount);

			if (!pBlock->pChunk->FreeBlock())
			{
				FreeChunk(pBlock->pChunk);
			}
		}

		void AddRef(T* data)
		{
			DWORD *ptr = (DWORD*)data;
			ptr -= 4;
			cMemoryChunk::st_CHUNK_BLOCK * pBlock = (cMemoryChunk::st_CHUNK_BLOCK*)ptr;

			if (pBlock->checkCode != en_OffBit)
			{
				return;
			}

			InterlockedIncrement(&_blockAllocCount);
			pBlock->pChunk->AddBlockRef();
		}

		UINT64 GetUsedBlockCount(void)
		{
			//return _memoryPool->GetUsedBlockCount();
			return _blockAllocCount;
		}

		UINT64 GetTotalBlockCount(void)
		{
			//return _memoryPool->GetTotalBlockCount();
			return _memoryPool->GetTotalBlockCount() * cMemoryChunk::en_MAX_COUNT;
		}

		bool SetTlsIndex(void)
		{
			if ((_tlsIndex = TlsAlloc()) == TLS_OUT_OF_INDEXES)
				return false;
			return true;
		}

		cMemoryChunk* GetChunkPtr(void)
		{
			cMemoryChunk *pChunk = (cMemoryChunk*)TlsGetValue(_tlsIndex);
			return pChunk;
		}

		bool FreeTlsIndex(void)
		{
			return TlsFree(_tlsIndex);
		}

		void AllocChunk(void)
		{
			cMemoryChunk* pChunk;
			pChunk = _memoryPool->Alloc();
			pChunk->Init();

			TlsSetValue(_tlsIndex, pChunk);
		}

		void FreeChunk(cMemoryChunk *pChunk)
		{
			_memoryPool->Free(pChunk);
		}

		DWORD	_tlsIndex;
	private:
		cMemoryPool<cMemoryChunk, cMemoryChunk> *_memoryPool;
		UINT64	_blockAllocCount;
	};
};