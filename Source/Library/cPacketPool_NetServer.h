#pragma once
namespace TUNJI_LIBRARY
{
	template <typename T> class cMemoryPoolTLS;
	class cNetPacketPool
	{
	public:
		friend class CMMOServer;
		friend class CNetServer;
		friend class CNetSession;
		friend class CMonitorClient;

		enum en_PACKET
		{
			en_BUFSIZE_DEFALUT = 1500, en_HEADERSIZE_DEFAULT = 5
		};

		enum en_ERROR_CODE
		{
			en_NONE = 0, en_DECODE_ERROR, en_DESERIALIZE_ERROR
		};

		struct packetException
		{
			WCHAR errLog[256];
		};

		cNetPacketPool();	//생성자
		cNetPacketPool(cNetPacketPool & cSrcPacket);				//복사 생성자	
		cNetPacketPool(int bufsize);
		virtual ~cNetPacketPool();								//파괴자

															/*연산자 오퍼레이터 멤버*/

															//	넣기
		cNetPacketPool&	operator=(cNetPacketPool &cSrcPacket);
		cNetPacketPool&	operator<<(BYTE byValue);
		cNetPacketPool&	operator<<(WORD wValue);
		cNetPacketPool&	operator<<(DWORD dwValue);
		cNetPacketPool&	operator<<(WCHAR chValue);
		cNetPacketPool&	operator<<(UINT64 uiValue);
		cNetPacketPool&	operator<<(char chValue);
		cNetPacketPool&	operator<<(int iValue);
		cNetPacketPool&	operator<<(unsigned int uiValue);
		cNetPacketPool&	operator<<(long lValue);
		cNetPacketPool&	operator<<(long long dlValue);
		cNetPacketPool&	operator<<(float fValue);
		cNetPacketPool&	operator<<(double dValue);
		cNetPacketPool&	operator<<(long double ldValue);

		//	빼기
		cNetPacketPool&	operator >> (BYTE &byValue);
		cNetPacketPool&	operator >> (WORD &wValue);
		cNetPacketPool&	operator >> (DWORD &dwValue);
		cNetPacketPool&	operator >> (WCHAR &chValue);
		cNetPacketPool&	operator >> (UINT64 &uiValue);
		cNetPacketPool&	operator >> (char &chValue);
		cNetPacketPool&	operator >> (int &iValue);
		cNetPacketPool&	operator >> (unsigned int &uiValue);
		cNetPacketPool&	operator >> (long &lValue);
		cNetPacketPool&	operator >> (long long &dlValue);
		cNetPacketPool&	operator >> (float &fValue);
		cNetPacketPool&	operator >> (double &dValue);
		cNetPacketPool&	operator >> (long double &ldValue);


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

		static cNetPacketPool*		Alloc(void);
		static void				Free(cNetPacketPool *pPacket);
		static void				AddRef(cNetPacketPool *pPacket);
		static int				GetUsedPoolSize(void);
		static int				GetTolalPoolSize(void);
		static bool				ConstructChunkPool(void);
		static void				DestructChunkPool(void);
		static void				AllocPacketChunk(void);
		static void				FreePacketChunk(void);

	private:
	#pragma pack(push,1)
		struct st_NET_HEADER
		{
			BYTE	checkCode;
			WORD	len;
			BYTE	randomCode;
			BYTE	checkSum;
		};
	#pragma pack(pop)

		void			Encode(BYTE, BYTE, BYTE);
		void			Decode(BYTE, BYTE, BYTE);
		void			Custom_SetNetHeader(st_NET_HEADER header);

		static			cMemoryPoolTLS<cNetPacketPool> *_packetMemoryPool;

		char	*_chpBuff;				//동적할당 포인터
		st_NET_HEADER	*_pHeader;
		int		_iBufSize;
		int		_iUsedSize;
		int		_iRear;
		int		_iFront;
		bool	_bEncoded;
		en_ERROR_CODE	_errorCode;
	};
}