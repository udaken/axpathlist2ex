

#include "axpathlist2ex.cpp"

#include <io.h>
#include <fcntl.h> 
#include <assert.h>
#include <fstream>
#include <sstream>
#include <codecvt>
#include <Shlwapi.h>

#include <crtdbg.h>

#define _CRTDBG_MAP_ALLOC
#define new  ::new(_NORMAL_BLOCK, __FILE__, __LINE__)

const BYTE rawData[3579] = {
	0x44, 0x3A, 0x5C, 0x44, 0x6F, 0x77, 0x6E, 0x6C, 0x6F, 0x61, 0x64, 0x73,
	0x5C, 0x4E, 0x47, 0x2E, 0x72, 0x61, 0x62, 0x62, 0x69, 0x74, 0x8E, 0x64,
	0x8E, 0x96, 0x95, 0xE5, 0x8F, 0x57, 0x92, 0x86, 0x20, 0x2D, 
};

const BYTE rawData2[] = "# coding: shift-jis\r\aaaa";


int main()
{
	using namespace std;
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	{
		HANDLE hFile = ::CreateFile(L"shiftjis.txt", 0, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		::IsSupported("", (DWORD)hFile);

		auto p = ::PathFindFileName(L"x");
		assert(p);
	}
	{
		auto buf = new char[100 * 1024 * 1024]();
		buf[0] = 'A';
		buf[1] = '\n';
		string s(buf, buf + 100 * 1024 * 1024);
		istringstream is(s,std::istringstream::binary);
		//is.rdbuf()->pubsetbuf(buf, 100 * 1024 * 1024);
		delete buf;
		is.imbue(std::locale(std::locale("", LC_CTYPE)));
		string line;
		 std::getline(is, line);
		 auto c = is.get();
		
		Sleep(0);
	}
	static_assert(sizeof(wchar_t) == 2, "error.");//Linux‚Å‚Í‚Â‚©‚¤cvtˆá‚¤‚©‚ç’¼‚µ‚Ä‚­‚ê
	std::wstring buf;
	{
		std::wifstream file;
		file.open("utf8bom.txt");
		file.imbue(std::locale(std::locale(""), new std::codecvt_utf8_utf16<wchar_t, 0x10ffff, std::consume_header>()));
		if (!file) return -1;
		std::getline(file, buf);
	}
	buf.clear();
	{
		std::wifstream file;
		file.open("utf8.txt");
		file.imbue(std::locale(std::locale(""), new std::codecvt_utf8_utf16<wchar_t, 0x10ffff, std::consume_header>()));
		if (!file) return -1;
		std::getline(file, buf);
	}
	buf.clear();
	{
		std::wifstream file;
		file.open("shiftjis.txt");
		file.imbue(std::locale(std::locale("", LC_CTYPE)));
		if (!file) return -1;
		std::getline(file, buf);
	}
	buf.clear();
	buf.clear();
	{
		std::wifstream file;
		file.open("utf16.txt");
		file.imbue(std::locale(std::locale(""), new std::codecvt_utf16<wchar_t, 0x10ffff, static_cast<std::codecvt_mode>(std::little_endian | std::consume_header)>));
		if (!file) return -1;
		std::getline(file, buf);
	}

	assert(detectEncoding(rawData2, _countof(rawData2)));
	assert(detectEncoding(rawData, _countof(rawData)));
}