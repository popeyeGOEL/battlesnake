#include <Windows.h>
#include "Text_Parser.h"

namespace TUNJI_LIBRARY
{
	bool cParse::LoadFile(WCHAR *chpstr)
	{
		int iTextLength;
		errno_t err;
		err = _wfopen_s(&fp, chpstr, L"rb");
		if (err != 0)
			return false;

		fseek(fp, 0, SEEK_END);
		iTextLength = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		_arrText = (WCHAR*)malloc(sizeof(WCHAR) * iTextLength);
		_ptr = _arrText;
		fread(_arrText, iTextLength, 1, fp);
		return true;

	}

	void cParse::RewindSection(void)
	{
		_ptr = _arrText;
		_iSectionStart = 0;
	}

	bool cParse::FindNextBlock(void)
	{
		int iCnt = 0;

		while(*_ptr != L'\0')
		{
			if(*_ptr == L':')
			{
				++_ptr;
				_iSectionStart = _ptr - _arrText;
				return true;
			}
			++_ptr;
		}

		return false;
	}

	bool cParse::FindSection(WCHAR *szName)
	{
		WCHAR *chpBuff = nullptr;;
		WCHAR chWord[256] = {0,};
		int iLength = 0;

		//ã���� �ϴ� �ܾ ���ö����� ��� ã�� ���̹Ƿ� while������ �˻� 
		while (FindNextBlock())
		{
			if (SkipNoneCommand() == false)
				return false;

			GetNextWord(&chpBuff, &iLength);

			//Word ���ۿ� ã�� �ܾ �����Ѵ�. 
			memset(chWord, 0, sizeof(WCHAR) * 256);
			memcpy(chWord, chpBuff, iLength);
			//���ڷ� �Է� ���� �ܾ�� ������ �˻��Ѵ�. 
			if(0 == wcscmp(szName, chWord))
			{
				_iSectionStart = _ptr - _arrText;
				return true;
			}
		}
		return false;
	}

	// �����̽�, ��,�����ڵ�, �ּ����� ó���Ѵ�
	bool cParse::SkipNoneCommand(void)
	{
		while (*_ptr != L'\0')
		{
			// 0x20 = space, 0x08 = backspace, 0x09 tab, 0x0a line feed , 0x0d carriage return 
			if (*_ptr == L' ' || *_ptr == L'\b' || *_ptr == L'\t' || *_ptr == L'\n' || *_ptr == L'\r')
				_ptr++;
			// �ּ��� ������ ���๮�ڰ� ���ö� ���� ������ ������.
			else if (*_ptr == L'/' && *(_ptr + 1) == L'/')
			{
				while (*_ptr != L'\n')
					_ptr++;
			}
			else if (*_ptr == L'/' && *(_ptr + 1) == L'*')
			{
				while (!(*(_ptr - 1) == L'*' && *_ptr == L'/'))
					_ptr++;
			}
			else
				return true;
		}
		return false;

	}

	// ���� �ܾ ���ö����� ã�´�. 
	bool cParse::GetNextWord(WCHAR **chppBuffer, int *ipLength)
	{
		int iCnt = 0;
		if (SkipNoneCommand() == TRUE)
			*chppBuffer = _ptr;
		else
			return false;

		while (*_ptr != L'\0')
		{
			if (*_ptr == L',' || *_ptr == L' ' || *_ptr == L'\b' ||
				*_ptr == L'\t' || *_ptr == L'\n' || *_ptr == L'\r')
				break;
			else if (*_ptr == L'"')
			{
				*chppBuffer = ++_ptr;
				while (*_ptr != L'"')
				{
					_ptr++;
					iCnt++;
				}
				_ptr++;
				break;
			}
			else if(*_ptr == L'}')
			{
				_ptr -= iCnt;
				return false;
			}

			_ptr++;
			iCnt++;
		}

		*ipLength = iCnt * sizeof(WCHAR);
		return true;
	}



	bool cParse::GetValue(WCHAR *szName, int *ipValue)
	{
		WCHAR *chpBuff; 
		WCHAR chWord[256];
		int iLength;

		//ã���� �ϴ� �ܾ ���ö����� ��� ã�� ���̹Ƿ� while������ �˻� 
		while (GetNextWord(&chpBuff, &iLength))
		{
			//Word ���ۿ� ã�� �ܾ �����Ѵ�. 
			memset(chWord, 0, sizeof(WCHAR) * 256);
			memcpy(chWord, chpBuff, iLength);
			//���ڷ� �Է� ���� �ܾ�� ������ �˻��Ѵ�. 
			if(0 == wcscmp(szName, chWord))
			{
				//�´ٸ� �ٷ� �ڿ� = �� ã�´�. 
				if (GetNextWord(&chpBuff, &iLength))
				{
					memset(chWord, 0, sizeof(WCHAR) * 256);
					memcpy(chWord, chpBuff, iLength);
					if(0== wcscmp(chWord, L"="))
					{
						// = ������ ������ �κ��� ��´�.
						if (GetNextWord(&chpBuff, &iLength))
						{
							memset(chWord, 0, sizeof(WCHAR) * 256);
							memcpy(chWord, chpBuff, iLength);
							*ipValue = _wtoi(chWord);
							_ptr = _arrText + _iSectionStart;
							return true;
						}
						break;
					}
				}
				break;
			}
		}
		_ptr = _arrText + _iSectionStart;
		return false;
	}


	bool cParse::GetValue(WCHAR *szName, WCHAR *chpValue)
	{
		WCHAR *chpBuff;
		WCHAR chWord[256];
		int iLength;

		//ã���� �ϴ� �ܾ ���ö����� ��� ã�� ���̹Ƿ� while������ �˻� 
		while (GetNextWord(&chpBuff, &iLength))
		{
			//Word ���ۿ� ã�� �ܾ �����Ѵ�. 
			memset(chWord, 0, sizeof(WCHAR) * 256);
			memcpy(chWord, chpBuff, iLength);
			//���ڷ� �Է� ���� �ܾ�� ������ �˻��Ѵ�. 
			if( 0 == wcscmp(szName, chWord))
			{
				//�´ٸ� �ٷ� �ڿ� = �� ã�´�. 
				if (GetNextWord(&chpBuff, &iLength))
				{
					memset(chWord, 0, sizeof(WCHAR) * 256);
					memcpy(chWord, chpBuff, iLength);
					if(0 == wcscmp(chWord, L"="))
					{
						// = ������ ������ �κ��� ��´�.
						if (GetNextWord(&chpBuff, &iLength))
						{
							memcpy(chpValue, chpBuff, iLength);
							_ptr = _arrText + _iSectionStart;
							return true;
						}
						break;
					}
				}
				break;
			}
		}
		_ptr = _arrText + _iSectionStart;
		return false;
	}

	void cParse::CloseFile(void)
	{
		free(_arrText);
		fclose(fp);
	}
}