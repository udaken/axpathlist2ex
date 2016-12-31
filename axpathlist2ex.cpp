#include "stdafx.h"

#include <windows.h>
#include <comip.h>
#include <comdef.h>
#include <mlang.h>
#include <Shlwapi.h>
#pragma comment(lib,"Shlwapi.lib")

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>

#include <array>
#include <vector>
#include <string>
#include <codecvt>
#include <algorithm> 
#include <functional> 
#include <cctype>
#include <functional> 
#include <fstream>

#define CP_932 932
#define CP_UNICODE 1200

const static BYTE UFT8_BOM[] = { 0xEF,0xBB,0xBF };
const static BYTE UFT16LE_BOM[] = { 0xFF,0xFE };


_COM_SMARTPTR_TYPEDEF(IMultiLanguage2, __uuidof(IMultiLanguage2));

extern HMODULE g_hModule;

class bad_optional_access : public std::logic_error
{
public:
	bad_optional_access(const char*msg)
		: logic_error(msg)
	{

	}
};
template <class T>
class optional : public std::tuple<bool, T>
{
private:
	typedef std::tuple<bool, T> base;
public:
	typedef T value_type;

	optional(const T &value)
		: base(true, value)
	{
	}
	optional()
		: base(false, {})
	{
	}
	optional(const base& value)
		: base(value)
	{
	}
	bool has_value() const
	{
		return std::get<0>(*this);
	}
	explicit operator bool() const
	{
		return has_value();
	}
	T& value() &
	{
		if (!has_value())
			throw bad_optional_access("object hasn't value");
		return std::get<1>(*this);
	}
	const T & value() const &
	{
		if (!has_value())
			throw bad_optional_access("object hasn't value");
		return std::get<1>(*this);
	}
	const T* operator->() const
	{
		if (!has_value())
			throw bad_optional_access("object hasn't value");
		return &get<1>();
	}
	T* operator->()
	{
		if (!has_value())
			throw bad_optional_access("object hasn't value");
		return &get<1>();
	}
	const T& operator*() const&
	{
		return value();
	}
	T& operator*() &
	{
		return value();
	}
};
template <class T>
static inline void ltrim(std::basic_string<T> &s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(),
		std::not1(std::ptr_fun<int, int>(std::isspace))));
}

template <class T = char>
T *strnchr(const T *s, size_t count, T c)
{
	for (; count-- && *s != '\0'; ++s)
	{
		if (*s == c)
		{
			return const_cast<T*>(s);
		}
	}
	return nullptr;
}
// エラーコード
#define SPI_SUCCESS			0		// 正常終了
#define SPI_NOT_IMPLEMENT	(-1)	// その機能はインプリメントされていない
#define SPI_USER_CANCEL		1		// コールバック関数が非0を返したので展開を中止した
#define SPI_UNKNOWN_FORMAT	2		// 未知のフォーマット
#define SPI_DATA_BROKEN		3		// データが壊れている
#define SPI_NO_MEMORY		4		// メモリーが確保出来ない
#define SPI_MEMORY_ERR		5		// メモリーエラー（Lock出来ない、等）
#define SPI_FILE_READ_ERR	6		// ファイルリードエラー
#define SPI_RESERVED_ERR	7		// （予約）
#define SPI_INTERNAL_ERR	8		// 内部エラー

#include <pshpack1.h>
// ファイル情報構造体
struct fileInfo
{
	unsigned char	method[8];		// 圧縮法の種類
	unsigned long	position;		// ファイル上での位置
	unsigned long	compsize;		// 圧縮されたサイズ
	unsigned long	filesize;		// 元のファイルサイズ
	time_t			timestamp;		// ファイルの更新日時
	char			path[200];		// 相対パス
	char			filename[200];	// ファイルネーム
	unsigned long	crc;			// CRC
};
#include <poppack.h>

static std::string w2string(LPCWSTR lpszStr, DWORD dwFlags = WC_NO_BEST_FIT_CHARS)
{
	size_t bufLen = wcslen(lpszStr) * 2 + 1;
	std::vector<CHAR> buf(bufLen, '\0');
	int ret = ::WideCharToMultiByte(CP_ACP, dwFlags, lpszStr, -1, buf.data(), bufLen, nullptr, nullptr);
	if (ret == 0 && ::GetLastError() == ERROR_INSUFFICIENT_BUFFER)
	{
		buf.reserve(buf.capacity() + 256);
		::WideCharToMultiByte(CP_ACP, dwFlags, lpszStr, -1, buf.data(), bufLen, nullptr, nullptr);
	}
	return buf.data();
}

static std::wstring a2wstring(LPCSTR lpszStr, DWORD dwFlags = 0)
{
	size_t bufLen = strlen(lpszStr) + 1;
	std::vector<WCHAR> buf(bufLen, L'\0');
	int ret = ::MultiByteToWideChar(CP_ACP, dwFlags, lpszStr, -1, buf.data(), bufLen);
	if (ret == 0 && ::GetLastError() == ERROR_INSUFFICIENT_BUFFER)
	{
		buf.reserve(buf.capacity() + 256);
		::MultiByteToWideChar(CP_ACP, dwFlags, lpszStr, -1, buf.data(), bufLen);
	}
	return buf.data();
}

class Config
{
public:
	Config()
	{
		TCHAR buf[MAX_PATH];
		::GetModuleFileName(g_hModule, buf, _countof(buf));
		::PathRenameExtension(buf, TEXT(".ini"));
		_path = buf;
	}
	std::string getExtension() const
	{
		WCHAR buf[256];
		::GetPrivateProfileString(TEXT("AXPATHLIST2EX"), TEXT("EXTENSION"), TEXT("*.sz7"), buf, _countof(buf), _path.c_str());
		return w2string(buf);
	}
	UINT getOverrideCodePage() const
	{
		return ::GetPrivateProfileInt(TEXT("AXPATHLIST2EX"), TEXT("OVERRIDE_CODEPAGE"), CP_ACP, _path.c_str());
	}
private:
	std::wstring _path;
};


//////////////////////////////////////////////////////////////////////////////
// Plug-in API
//////////////////////////////////////////////////////////////////////////////

int __stdcall GetPluginInfo(int infono, LPSTR buf, int buflen)
{
	if (buf == NULL || buflen <= 0)
		return 0;

	Config config;
	auto ext = config.getExtension();

	if (infono == 0)
		return ::strlen(::lstrcpynA(buf, "00AM", buflen));
	else if (infono == 1)
		return ::strlen(::lstrcpynA(buf, "axpathlist2ex", buflen));
	else if (infono == 2)
		return ::strlen(::lstrcpynA(buf, ext.c_str(), buflen));
	else if (infono == 3)
		return ::strlen(::lstrcpynA(buf, ("pathlist (" + ext + ")").c_str(), buflen));

	return 0;
}


static optional<UINT> codePageFromName(LPCWSTR name, IMultiLanguage2 *pIML2)
{
	MIMECSETINFO mimeInfo;
	HRESULT hr = pIML2->GetCharsetInfo(_bstr_t(name), &mimeInfo);
	if (SUCCEEDED(hr))
	{
		return mimeInfo.uiInternetEncoding;
	}

}


static optional<UINT> detectEncoding(const BYTE *input, size_t len)
{
	Config config;
	UINT cp = config.getOverrideCodePage();
	if (cp != CP_ACP)
		return cp;

	optional<UINT> o;
	if (memcmp(input, UFT8_BOM, _countof(UFT8_BOM)) == 0)
	{
		return CP_UTF8;
	}
	else if (memcmp(input, UFT16LE_BOM, _countof(UFT16LE_BOM)) == 0)
	{
		return CP_UNICODE;
	}
#if 0
	HRESULT hr = ::CoInitialize(NULL);
	assert(SUCCEEDED(hr));

	IMultiLanguage2Ptr pIML2;
	hr = pIML2.CreateInstance(CLSID_CMultiLanguage);
	if (FAILED(hr))
	{
		return false;
	}

	INT cSrcSize = len;
	DetectEncodingInfo detectInfo[1] = { 0 };
	INT nScores = _countof(detectInfo);
	hr = pIML2->DetectInputCodepage(MLDETECTCP_DBCS, GetACP(), (CHAR*)input, &cSrcSize, detectInfo, &nScores);
	if (SUCCEEDED(hr))
	{
		return detectInfo[0].nCodePage;
	}
#endif
	return CP_ACP;
}

static optional<UINT> detectEncoding(LPCSTR path)
{
	std::ifstream stream;
	stream.open(path, std::ios::binary);

	if (stream.bad())
		return optional<UINT>();

	std::array<char, 2048> buf{};
	stream.read(buf.data(), buf.size());

	return detectEncoding((const BYTE*)buf.data(), buf.size());
}

static std::wifstream openText(LPCSTR path)
{
	auto encoding = detectEncoding(path);
	std::wifstream file;
	file.open(path);
	switch (encoding.value())
	{
	case CP_UTF8:
		file.imbue(std::locale(std::locale(""), new std::codecvt_utf8_utf16<wchar_t, 0x10ffff, std::consume_header>()));
		break;
	case CP_UNICODE:
		file.imbue(std::locale(std::locale(""), new std::codecvt_utf16<wchar_t, 0x10ffff, static_cast<std::codecvt_mode>(std::little_endian | std::consume_header)>));
		break;
	default:
		file.imbue(std::locale(std::locale("", LC_CTYPE)));
	}

	return std::move(file);
}

int __stdcall IsSupported(LPSTR filename, DWORD dw)
{
	try
	{
		Config config;
		if (::PathMatchSpecA(filename, config.getExtension().c_str()))
		{
			return TRUE;
		}
#if 0	// 拡張子にかかわらずIsSupported()がコールされると、正しく判定できないので文字コードの判定は行わない。
		const BYTE*	pPathList = nullptr;
		if (HIWORD(dw)) // バッファへのポインタ
		{
			pPathList = (const BYTE*)dw;

			if (pPathList == nullptr)
				return FALSE;

			if (detectEncoding(pPathList, 2048))
				return TRUE;
		}
		else // ファイルハンドル
		{
			HANDLE hFile = (HANDLE)dw;

			CHAR path[MAX_PATH];
			if (!::GetFinalPathNameByHandleA(hFile, path, _countof(path), 0))
			{
				return FALSE;
			}
			if (detectEncoding(path))
				return TRUE;

		}
#endif	
		return FALSE;
	}
	catch (const std::exception&)
	{
		return FALSE;
	}

}
struct Context
{
	Context()
		: useFileName(false)
	{
	}
	bool useFileName;
	std::wstring relativePath;
};

#define DIRECTIVE_DIRECTORY L"|directory:"
#define DIRECTIVE_USEORIGINALFILENAME L"|useFileName"

static bool processDirective(LPCWSTR path, Context &context)
{
	if (path[0] != L'|')
		return false;

	if (_wcsicmp(path, DIRECTIVE_USEORIGINALFILENAME) == 0)
	{
		context.useFileName = true;
		context.relativePath.clear();
	}
	else if (_wcsnicmp(path, DIRECTIVE_DIRECTORY, wcslen(DIRECTIVE_DIRECTORY)) == 0)
	{
		context.useFileName = false;
		context.relativePath = (path + wcslen(DIRECTIVE_DIRECTORY));
	}
	return true;
}

enum class Action
{
	Break,
	Continue,
};

template<class TCallback>
static int iterateArchive(LPCSTR buf, TCallback callback)
{
	Config config;
	Context context;
	context.useFileName = true;

	auto file = openText(buf);

	size_t cnt;
	for (cnt = 0; file.good(); )
	{
		std::wstring line;
		std::getline(file, line);

		if (line.empty())
			continue;

		if (processDirective(line.c_str(), context))
			continue;

		WIN32_FIND_DATA fad = {};

		WCHAR parent[MAX_PATH];
		wcscpy_s(parent, line.c_str());
		::PathRemoveFileSpec(parent);

		Action action;
		if (HANDLE hFind = ::FindFirstFile(line.c_str(), &fad))
		{
			do
			{
				if (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
					continue;

				action = callback(context, cnt, parent, fad);
				if (Action::Break == action)
					break;

				cnt++;
			} while (::FindNextFile(hFind, &fad));
			::FindClose(hFind);
		}

		if (action == Action::Break)
			break;
	}
	if (file.fail())
		return SPI_FILE_READ_ERR;

	return SPI_SUCCESS;
}

static fileInfo findData2FileInfo(Context context, size_t index, const WIN32_FIND_DATA &fad)
{
	ULARGE_INTEGER uli = { fad.ftLastWriteTime.dwLowDateTime, fad.ftLastWriteTime.dwHighDateTime };
	time_t		timestamp = (time_t)((uli.QuadPart - 0x19DB1DED53E8000) / 10000000);

	fileInfo fi = {};
	fi.position = index;
	fi.compsize = fi.filesize = fad.nFileSizeLow;
	fi.method[0] = 'a';
	fi.timestamp = timestamp;

	if (context.useFileName)
	{
		sprintf_s(fi.path, "%09u\\", index + 1);

		::WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS,
			fad.cFileName, -1, fi.filename, _countof(fi.filename), NULL, NULL);
	}
	else
	{
		LPCWSTR ext = ::PathFindExtension(fad.cFileName);
		sprintf_s(fi.filename, "%09u%S", index + 1, ext ? ext : L"");
		if (!context.relativePath.empty())
		{
			::WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS,
				context.relativePath.c_str(), -1, fi.path, _countof(fi.path), NULL, NULL);
		}
	}
	return fi;
}

int __stdcall GetArchiveInfo(LPCSTR buf, long len, unsigned int flag, HLOCAL * lphInf)
{
	if (lphInf == nullptr)
		return SPI_INTERNAL_ERR;
	if (flag & 0b000111)
		return SPI_NOT_IMPLEMENT;	// メモリ上のイメージには対応しない

	try
	{
		std::vector<fileInfo> list;
		int ret = iterateArchive(buf, [&](Context context, size_t index, const std::wstring &parent, const WIN32_FIND_DATA &fad)
		{
			list.push_back(findData2FileInfo(context, index, fad));

			return Action::Continue;
		});

		*lphInf = ::LocalAlloc(LPTR, sizeof(fileInfo) * (list.size() + 1));
		if (nullptr == *lphInf)
			return SPI_NO_MEMORY;

		memcpy((*lphInf), list.data(), list.size() * sizeof(fileInfo));

		return SPI_SUCCESS;
	}
	catch (const std::exception&)
	{
		return SPI_INTERNAL_ERR;
	}
}

int __stdcall GetFileInfo(LPCSTR buf, long len, LPSTR filename, unsigned int flag, fileInfo * lpInfo)
{
	if (lpInfo == nullptr || buf == nullptr)
		return SPI_INTERNAL_ERR;
	if (flag & 0b000111)
		return SPI_NOT_IMPLEMENT;	// メモリ上のイメージには対応しない

	try
	{
		int ret = iterateArchive(buf, [&](Context context, size_t index, const std::wstring &parent, const WIN32_FIND_DATA &fad)
		{
			auto position = 0;
			if (context.useFileName)
			{
				//先頭のディレクトリ名をインデックスにする
				position = atol(filename) - 1;
			}
			else
			{
				position = atol(::PathFindFileNameA(filename)) - 1;
			}
			if (index == position)
			{
				auto fi = findData2FileInfo(context, index, fad);
				*lpInfo = fi;
				return Action::Break;
			}
			return Action::Continue;
		});

		return SPI_SUCCESS;
	}
	catch (const std::exception&)
	{
		return SPI_INTERNAL_ERR;
	}
}

static BOOL readAllBytes(LPCWSTR path, LPVOID data, SIZE_T size)
{
	auto hFile = ::CreateFile(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}
	
	DWORD dwNumberOfBytesRead;
	if (!::ReadFile(hFile, data, size, &dwNumberOfBytesRead, nullptr) || dwNumberOfBytesRead != dwNumberOfBytesRead)
	{
		::CloseHandle(hFile);
		return FALSE;
	}

	::CloseHandle(hFile);
	return TRUE;
}

int __stdcall GetFile(LPCSTR buf, long len, LPSTR dest, unsigned int flag, FARPROC progressCallback, long lData)
{
	if (buf == nullptr || dest == nullptr)
		return SPI_INTERNAL_ERR;
	if (flag & 0b000111)
		return SPI_NOT_IMPLEMENT;	// メモリ上のイメージには対応しない

	try
	{
		size_t position = len;
		int ret = SPI_INTERNAL_ERR;

		iterateArchive(buf, [&](Context context, size_t index, const std::wstring &parent, const WIN32_FIND_DATA &fad)
		{
			if (index != position)
			{
				return Action::Continue;
			}

			WCHAR path[MAX_PATH];
			::PathCombine(path, parent.c_str(), fad.cFileName);

			if (flag & 0b000011100000000) // メモリ上のイメージ 
			{
				if (fad.nFileSizeHigh > 0)
				{
					ret = SPI_NO_MEMORY;
					return Action::Break;
				}
				
				HLOCAL hBuf = ::LocalAlloc(LPTR, fad.nFileSizeLow);
				if (hBuf == nullptr)
				{
					ret = SPI_MEMORY_ERR;
					return Action::Break;
				}
				if (!readAllBytes(path, hBuf, fad.nFileSizeLow))
				{
					::LocalFree(hBuf);
					ret = SPI_FILE_READ_ERR;
					return Action::Break;
				}

				::LocalUnlock(hBuf);
				*reinterpret_cast<HLOCAL*>(dest) = hBuf;
				ret = SPI_SUCCESS;
			}
			else // ディスクファイル 
			{
				WCHAR newPath[MAX_PATH];

				LPCWSTR ext = ::PathFindExtension(fad.cFileName);
				swprintf_s(newPath, L"%S\\%09u%S", dest, index + 1, ext ? ext : L"");

				ret = ::CopyFile(path, newPath, FALSE) ? SPI_SUCCESS : SPI_FILE_READ_ERR;
			}
			return Action::Break;
		});

		return ret;
	}
	catch (const std::exception&)
	{
		return SPI_INTERNAL_ERR;
	}
	return 0;
}


