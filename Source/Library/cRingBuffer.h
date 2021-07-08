#pragma once
namespace TUNJI_LIBRARY
{
	class cRingBuffer
	{
	public:
		enum en_SIZE { en_DEFAULT_SIZE = 5000 };
		cRingBuffer();
		cRingBuffer(int iBufferSize);
		~cRingBuffer();

		int GetTotalSize(void);						//총 버퍼 사이즈 
		int GetUsedSize(void);						//현재 사용중인 버퍼 사이즈 
		int GetEmptySize(void);						//현재 남은 버퍼 사이즈 
		int GetConstantReadSize(void);				//외부에서 끊김없이 읽을 수 있는 길이
		int GetConstantWriteSize(void);				//외부에서 끊김없이 쓸 수 있는 길이

		int Enqueue(char *chp, int iSize);			//데이터 넣기 
		int Dequeue(char *chp, int iSize);			//데이터 가져오기
		int Peek(char *chp, int iSize);				//데이터 읽기
		int GetFrontPos(void);
		void MoveFrontforDelete(int iSize);			//길이만큼 삭제 위치 이동 
		int	 MoveRearforSave(int iSize);			//길이만큼 쓰기 위치 이동

		void ClearBuffer(void);						//버퍼 비우기

		char *GetStartptr(void);					//버퍼 포인터 얻기
		char *GetReadptr(void);						//읽기 위치 포인터 얻기
		char *GetWriteptr(void);					//쓰기 위치 포인터 얻기

	private:
		void Initial(int iBufferSize);
		char *_chpBuff;
		int	_iFront;
		int _iRear;
		int _iBufferSize;
	};
}