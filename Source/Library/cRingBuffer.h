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

		int GetTotalSize(void);						//�� ���� ������ 
		int GetUsedSize(void);						//���� ������� ���� ������ 
		int GetEmptySize(void);						//���� ���� ���� ������ 
		int GetConstantReadSize(void);				//�ܺο��� ������� ���� �� �ִ� ����
		int GetConstantWriteSize(void);				//�ܺο��� ������� �� �� �ִ� ����

		int Enqueue(char *chp, int iSize);			//������ �ֱ� 
		int Dequeue(char *chp, int iSize);			//������ ��������
		int Peek(char *chp, int iSize);				//������ �б�
		int GetFrontPos(void);
		void MoveFrontforDelete(int iSize);			//���̸�ŭ ���� ��ġ �̵� 
		int	 MoveRearforSave(int iSize);			//���̸�ŭ ���� ��ġ �̵�

		void ClearBuffer(void);						//���� ����

		char *GetStartptr(void);					//���� ������ ���
		char *GetReadptr(void);						//�б� ��ġ ������ ���
		char *GetWriteptr(void);					//���� ��ġ ������ ���

	private:
		void Initial(int iBufferSize);
		char *_chpBuff;
		int	_iFront;
		int _iRear;
		int _iBufferSize;
	};
}