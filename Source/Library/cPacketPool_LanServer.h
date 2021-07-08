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

		cLanPacketPool();	//������
		cLanPacketPool(cLanPacketPool & cSrcPacket);				//���� ������	
		cLanPacketPool(int bufsize);
		virtual ~cLanPacketPool();								//�ı���

															/*������ ���۷����� ���*/

															//	�ֱ�
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

		//	����
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
		/*cMemoryPool�� ����ϰų�, cMemoryPoolTLS�� ����� �� �ִ�.*/
		static				cMemoryPoolTLS<cLanPacketPool> *_packetMemoryPool;

#pragma pack(push,1)
		struct st_LAN_HEADER
		{
			WORD payload;
		};


#pragma pack(pop)
		void			Custom_SetLanHeader(WORD payload);		//�������� ���̷ε� ���� 

		char	*_chpBuff;				//�����Ҵ� ������
		st_LAN_HEADER	*_pHeader;
		int		_iBufSize;
		int		_iUsedSize;
		int		_iRear;
		int		_iFront;
		int		_iHeaderSize;
		en_ERROR_CODE	_errorCode;
	};
}