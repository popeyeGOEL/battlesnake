#pragma once
namespace TUNJI_LIBRARY
{
	template <typename T> class cMemoryPoolTLS;
	class cLanPacketPool
	{
	public:
		friend class CLanServer;
		friend class CLanClient;

		enum en_PACKET
		{
			en_BUFSIZE_DEFALUT = 1024, en_HEADERSIZE_DEFAULT = 2
		};

		enum en_ERROR_CODE
		{
			en_NONE = 0, en_DECODE_ERROR, en_DESERIALIZE_ERROR
		};

		struct packetException
		{
			WCHAR errLog[256];
		};

		cLanPacketPool();	//생성자
		cLanPacketPool(cLanPacketPool & cSrcPacket);				//복사 생성자	
		cLanPacketPool(int bufsize);
		virtual ~cLanPacketPool();								//파괴자

															/*연산자 오퍼레이터 멤버*/

															//	넣기
		cLanPacketPool&	operator=(cLanPacketPool &cSrcPacket);
		cLanPacketPool&	operator<<(BYTE byValue);
		cLanPacketPool&	operator<<(WORD wValue);
		cLanPacketPool&	operator<<(DWORD dwValue);
		cLanPacketPool&	operator<<(WCHAR chValue);
		cLanPacketPool&	operator<<(UINT64 uiValue);
		cLanPacketPool&	operator<<(char chValue);
		cLanPacketPool&	operator<<(int iValue);
		cLanPacketPool&	operator<<(unsigned int uiValue);
		cLanPacketPool&	operator<<(long lValue);
		cLanPacketPool&	operator<<(long long dlValue);
		cLanPacketPool&	operator<<(float fValue);
		cLanPacketPool&	operator<<(double dValue);
		cLanPacketPool&	operator<<(long double ldValue);

		//	빼기
		cLanPacketPool&	operator >> (BYTE &byValue);
		cLanPacketPool&	operator >> (WORD &wValue);
		cLanPacketPool&	operator >> (DWORD &dwValue);
		cLanPacketPool&	operator >> (WCHAR &chValue);
		cLanPacketPool&	operator >> (UINT64 &uiValue);
		cLanPacketPool&	operator >> (char &chValue);
		cLanPacketPool&	operator >> (int &iValue);
		cLanPacketPool&	operator >> (unsigned int &uiValue);
		cLanPacketPool&	operator >> (long &lValue);
		cLanPacketPool&	operator >> (long long &dlValue);
		cLanPacketPool&	operator >> (float &fValue);
		cLanPacketPool&	operator >> (double &dValue);
		cLanPacketPool&	operator >> (long double &ldValue);


		/*일반 멤버*/
		void			clear(void);							//버퍼 지우기
		int				GetBufSize(void);						//버퍼 사이즈 얻기
		int				GetDataSize(void);						//사용중인 버퍼 사이즈 얻기 (헤더 제외)
		int				GetDataSizewithHeader(void);			//사용중인 버퍼 사이즈 얻기 (실제 헤더사이즈 포함)

		char*			GetBufptr(void);						//버퍼 포인터 얻기
		char*			GetWriteptr(void);						//쓰기 위치 버퍼 포인터 
		char*			GetReadptr(void);						//읽기 위치 버퍼 포인터
		int				MoveFrontforDelete(int iSize);			//Front 이동 삭제
		int				MoveRearforSave(int iSize);				//Rear 이동 저장

		int				Deserialize(char *chpDest, int iSize);	//데이터 출력
		int				Serialize(char *chpSrc, int iSrcSize);	//데이터 저장

		static cLanPacketPool*		Alloc(void);
		static void				Free(cLanPacketPool *pPacket);
		static void				AddRef(cLanPacketPool *pPacket);
		static int				GetUsedPoolSize(void);
		static int				GetTolalPoolSize(void);
		static bool				ConstructChunkPool(void);
		static void				DestructChunkPool(void);
		static void				AllocPacketChunk(void);
		static void				FreePacketChunk(void);

	private:
		/*cMemoryPool을 사용하거나, cMemoryPoolTLS를 사용할 수 있다.*/
		static				cMemoryPoolTLS<cLanPacketPool> *_packetMemoryPool;

#pragma pack(push,1)
		struct st_LAN_HEADER
		{
			WORD payload;
		};


#pragma pack(pop)
		void			Custom_SetLanHeader(WORD payload);		//랜서버용 페이로드 저장 

		char	*_chpBuff;				//동적할당 포인터
		st_LAN_HEADER	*_pHeader;
		int		_iBufSize;
		int		_iUsedSize;
		int		_iRear;
		int		_iFront;
		int		_iHeaderSize;
		en_ERROR_CODE	_errorCode;
	};
}