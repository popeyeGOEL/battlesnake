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

		cNetPacketPool();	//������
		cNetPacketPool(cNetPacketPool & cSrcPacket);				//���� ������	
		cNetPacketPool(int bufsize);
		virtual ~cNetPacketPool();								//�ı���

															/*������ ���۷����� ���*/

															//	�ֱ�
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

		//	����
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


		/*�Ϲ� ���*/
		void			clear(void);							//���� �����
		int				GetBufSize(void);						//���� ������ ���
		int				GetDataSize(void);						//������� ���� ������ ��� (��� ����)
		int				GetDataSizewithHeader(void);			//������� ���� ������ ��� (���� ��������� ����)

		char*			GetBufptr(void);						//���� ������ ���
		char*			GetWriteptr(void);						//���� ��ġ ���� ������ 
		char*			GetReadptr(void);						//�б� ��ġ ���� ������
		int				MoveFrontforDelete(int iSize);			//Front �̵� ����
		int				MoveRearforSave(int iSize);				//Rear �̵� ����

		int				Deserialize(char *chpDest, int iSize);	//������ ���
		int				Serialize(char *chpSrc, int iSrcSize);	//������ ����

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

		char	*_chpBuff;				//�����Ҵ� ������
		st_NET_HEADER	*_pHeader;
		int		_iBufSize;
		int		_iUsedSize;
		int		_iRear;
		int		_iFront;
		bool	_bEncoded;
		en_ERROR_CODE	_errorCode;
	};
}