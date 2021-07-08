#pragma 
#include <iostream>
namespace TUNJI_LIBRARY
{
	class cParse {
	public:
		cParse() : _iSectionStart(0), _arrText(nullptr), _ptr(nullptr) {}
		bool LoadFile(WCHAR *chpStr);
		void RewindSection(void);
		bool FindSection(WCHAR *szName);
		bool FindNextBlock(void);
		bool GetValue(WCHAR *szName, int *pValue);
		bool GetValue(WCHAR *szName, WCHAR *pValue);
		void CloseFile(void);
		~cParse() { fclose(fp); }

	private:
		bool SkipNoneCommand(void);
		bool GetNextWord(WCHAR **pBuffer, int *pLength);

		FILE *fp;
		int _iSectionStart;
		WCHAR *_arrText;
		WCHAR *_ptr;
	};
}
//
//void main(void)
//{
//	cParse parse;
//
//	int iValue1;
//	int iValue2;
//	char szText[30] = { 0, };
//
//	if (parse.LoadFile("config.txt") == false)
//	{
//		cout << "파일 열기에 실패하였습니다!" << endl;
//		return;
//	}
//
//	parse.FindSection("server2");
//
//	parse.GetValue("Version", &iValue1);
//	parse.GetValue("ServerBindIP", szText);
//	puts(szText);
//	cout << iValue1 << endl;
//
//	parse.FindSection("server1");
//	parse.GetValue("ServerBindIP", szText);
//	parse.GetValue("Version", &iValue2);
//
//	puts(szText);
//	cout << iValue2 << endl;
//
//}