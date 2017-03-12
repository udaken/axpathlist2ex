#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Windows ヘッダーから使用されていない部分を除外します。
#define STRICT
#define STRICT_CONST

#include <windows.h>
#include <comdef.h>
#include <comip.h>
#include <mlang.h>
#include <Shlwapi.h>
#pragma comment(lib,"Shlwapi.lib")

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <mbstring.h>

#include <array>
#include <vector>
#include <string>
#include <codecvt>
#include <algorithm>
#include <functional>
#include <cctype>
#include <functional>
#include <fstream>

#include "optional.hpp"
#include "utility.hpp"

#define CP_UNICODE 1200

const static BYTE UFT8_BOM[] = { 0xEF, 0xBB, 0xBF };
const static BYTE UFT16LE_BOM[] = { 0xFF, 0xFE };

_COM_SMARTPTR_TYPEDEF(IMultiLanguage2, __uuidof(IMultiLanguage2));

extern HMODULE g_hModule;

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

#ifdef _WIN64
typedef __time64_t		susie_time_t;
#else
typedef __time32_t		susie_time_t;
#endif

#include <pshpack1.h>
// ファイル情報構造体
struct fileInfo
{
    unsigned char	method[8];		// 圧縮法の種類
	ULONG_PTR		position;		// ファイル上での位置
	ULONG_PTR		compsize;		// 圧縮されたサイズ
	ULONG_PTR		filesize;		// 元のファイルサイズ
	susie_time_t	timestamp;		// ファイルの更新日時
	char			path[200];		// 相対パス
    char			filename[200];	// ファイルネーム
    unsigned long	crc;			// CRC
#ifdef _WIN64
									// 64bit版の構造体サイズは444bytesですが、実際のサイズは
									// アラインメントにより448bytesになります。環境によりdummyが必要です。
	char        dummy[4];
#endif
};
#include <poppack.h>

enum class SpiResult
{
    Success = SPI_SUCCESS,
    NotImplement = SPI_NOT_IMPLEMENT,
    UserCancel = SPI_USER_CANCEL,
    UnknownFormat = SPI_UNKNOWN_FORMAT,
    DataBroken = SPI_DATA_BROKEN,
    NoMemory = SPI_NO_MEMORY,
    MemoryError = SPI_MEMORY_ERR,
    FileReadError = SPI_FILE_READ_ERR,
    Resered = SPI_RESERVED_ERR,
    InternalError = SPI_INTERNAL_ERR,
};

enum class Action
{
    Break,
    Continue,
};

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

    BOOL getUseFilename() const
    {
        return ::GetPrivateProfileInt(TEXT("AXPATHLIST2EX"), TEXT("USE_FILENAME"), 0, _path.c_str());
    }

private:
    std::wstring _path;
};

int __stdcall GetPluginInfo(int infono, LPSTR buf, int buflen)
{
    if (buf == NULL || buflen <= 0)
    {
        return 0;
    }

    Config config;
    auto ext = config.getExtension();

    if (infono == 0)
    {
        return ::strlen(::lstrcpynA(buf, "00AM", buflen));
    }
    else if (infono == 1)
    {
        return ::strlen(::lstrcpynA(buf, "axpathlist2ex", buflen));
    }
    else if (infono == 2)
    {
        return ::strlen(::lstrcpynA(buf, ext.c_str(), buflen));
    }
    else if (infono == 3)
    {
        return ::strlen(::lstrcpynA(buf, ("pathlist (" + ext + ")").c_str(), buflen));
    }

    return 0;
}

static optional<UINT> codePageFromName(LPCWSTR name, IMultiLanguage2 *pIML2)
{
    MIMECSETINFO mimeInfo;
    HRESULT hr = pIML2->GetCharsetInfo(_bstr_t (name), &mimeInfo);

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
    {
        return cp;
    }

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
    hr = pIML2->DetectInputCodepage(MLDETECTCP_DBCS, GetACP(), (CHAR*) input, &cSrcSize, detectInfo, &nScores);

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
    {
        return optional<UINT>();
    }

    std::array<char, 2048> buf{};
    stream.read(buf.data(), buf.size());

    return detectEncoding((const BYTE*) buf.data(), buf.size());
}

static std::wifstream openText(LPCSTR path)
{
    auto encoding = detectEncoding(path);
    std::wifstream file;
    file.open(path);

    switch (encoding.value())
    {
    case CP_UTF8:
        file.imbue(std::locale(std::locale(""),
                               new std::codecvt_utf8_utf16<wchar_t, 0x10ffff, std::consume_header>()));
        break;

    case CP_UNICODE:
        file.imbue(std::locale(std::locale(""),
                               new std::codecvt_utf16 <wchar_t, 0x10ffff, static_cast<std::codecvt_mode>(std::little_endian | std::consume_header)>));
        break;

    case CP_ACP:
        file.imbue(std::locale(std::locale("", LC_CTYPE)));
        break;

    default:
        file.imbue(std::locale(std::locale("." + std::to_string(encoding.value()), LC_CTYPE)));
    }

    return file;
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

        if (HIWORD(dw))      // バッファへのポインタ
        {
            pPathList = (const BYTE*) dw;

            if (pPathList == nullptr)
            {
                return FALSE;
            }

            if (detectEncoding(pPathList, 2048))
            {
                return TRUE;
            }
        }
        else // ファイルハンドル
        {
            HANDLE hFile = (HANDLE) dw;

            CHAR path[MAX_PATH];

            if (!::GetFinalPathNameByHandleA(hFile, path, _countof(path), 0))
            {
                return FALSE;
            }

            if (detectEncoding(path))
            {
                return TRUE;
            }

        }

#endif
        return FALSE;
    }
    catch (const std::exception &)
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
    {
        return false;
    }

    if (_wcsicmp(path, DIRECTIVE_USEORIGINALFILENAME) == 0)
    {
        context.useFileName = true;
    }
    else if (_wcsnicmp(path, DIRECTIVE_DIRECTORY, wcslen(DIRECTIVE_DIRECTORY)) == 0)
    {
        context.relativePath = (path + wcslen(DIRECTIVE_DIRECTORY));
    }

    return true;
}


template<class TCallback>
static SpiResult iterateArchive(LPCSTR buf, TCallback callback)
{
    Config config;
    Context context;
    context.useFileName = config.getUseFilename() != 0;

    auto file = openText(buf);
    
    for (size_t cnt = 0; file.good() && cnt <= UINT32_MAX;)
    {
        std::wstring line;
        std::getline(file, line);

        if (line.empty())
        {
            continue;
        }

        if (processDirective(line.c_str(), context))
        {
            continue;
        }

        WCHAR path[MAX_PATH];

        if (::PathIsRelative(line.c_str()))
        {
            wcscpy_s(path, a2wstring(buf).c_str());
            ::PathRemoveFileSpec(path);
            ::PathCombine(path, path, line.c_str());
        }
        else
        {
            wcscpy_s(path, line.c_str());
        }

        WCHAR parent[MAX_PATH];
        wcscpy_s(parent, path);
        ::PathRemoveFileSpec(parent);

        std::vector<WIN32_FIND_DATA> files;
        auto action = Action::Continue;
        {
            WIN32_FIND_DATA fad = {};
            if (HANDLE hFind = ::FindFirstFile(path, &fad))
            {
                do
                {
                    if (wcscmp(fad.cFileName, L".") == 0 || wcscmp(fad.cFileName, L"..") == 0)
                    {
                        continue;
                    }

                    if (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    {
                        continue;
                    }

                    files.push_back(fad);
                } while (::FindNextFile(hFind, &fad));

                ::FindClose(hFind);
            }
        }

        std::stable_sort(files.begin(), files.end(), [](const WIN32_FIND_DATA &left, const WIN32_FIND_DATA &right) {
            return StrCmpLogicalW(left.cFileName, right.cFileName) < 0;
        });

        for(const auto fad : files)
        {
            action = callback(context, cnt, parent, fad);

            if (Action::Break == action)
            {
                break;
            }
            cnt++;
        }

        if (action == Action::Break)
        {
            break;
        }
    }

    if (file.fail())
    {
        return SpiResult::FileReadError;
    }

    return SpiResult::Success;
}

static fileInfo findData2FileInfo(Context &context, DWORD index, const WIN32_FIND_DATA &fad)
{
    ULARGE_INTEGER uli = { fad.ftLastWriteTime.dwLowDateTime, fad.ftLastWriteTime.dwHighDateTime };
    auto timestamp = (__time32_t)((uli.QuadPart - 0x19DB1DED53E8000) / 10000000);

    fileInfo fi = {};
    fi.position = index;
    fi.compsize = fi.filesize = fad.nFileSizeLow;
    fi.method[0] = 'a';
    fi.timestamp = timestamp;

    if (!context.relativePath.empty())
    {
        sprintf_s(fi.path, "%S\\", context.relativePath.c_str());
    }

    if (context.useFileName)
    {
        char buf[20+1];
        sprintf_s(buf, "%09u\\", index + 1);
        strcat_s(fi.path, buf);

        ::WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS,
                              fad.cFileName, -1, fi.filename, _countof(fi.filename), NULL, NULL);
    }
    else
    {
        LPCWSTR ext = ::PathFindExtension(fad.cFileName);
        sprintf_s(fi.filename, "%09u%S", index + 1, ext ? ext : L"");
    }

    return fi;
}

int __stdcall GetArchiveInfo(LPCSTR buf, long len, unsigned int flag, HLOCAL * lphInf)
{
    if (lphInf == nullptr)
    {
        return SPI_INTERNAL_ERR;
    }

    if (flag & 0b000111)
    {
        return SPI_NOT_IMPLEMENT;    // メモリ上のイメージには対応しない
    }

    try
    {
        std::vector<fileInfo> list;
        SpiResult ret = iterateArchive(buf, [&](Context & context, size_t index, const std::wstring & parent, const WIN32_FIND_DATA & fad)
        {
			assert(index <= UINT32_MAX);
            list.push_back(findData2FileInfo(context, static_cast<DWORD>(index), fad));

            return Action::Continue;
        });

        *lphInf = ::LocalAlloc(LPTR, sizeof(fileInfo) * (list.size() + 1));

        if (nullptr == *lphInf)
        {
            return SPI_NO_MEMORY;
        }

        memcpy((*lphInf), list.data(), sizeof(fileInfo) * list.size());

        return SPI_SUCCESS;
    }
    catch (const std::exception &)
    {
        return SPI_INTERNAL_ERR;
    }
}

int __stdcall GetFileInfo(LPCSTR buf, long len, LPSTR filename, unsigned int flag, fileInfo * lpInfo)
{
    if (lpInfo == nullptr || buf == nullptr)
    {
        return SPI_INTERNAL_ERR;
    }

    if (flag & 0b000111)
    {
        return SPI_NOT_IMPLEMENT;    // メモリ上のイメージには対応しない
    }

    try
    {
        SpiResult ret = iterateArchive(buf, [&](Context & context, size_t index, const std::wstring & parent, const WIN32_FIND_DATA & fad)
        {
            auto position = 0;

            unsigned char* sepPos = _mbschr((unsigned char*)filename, '\\');
            LPCSTR positionStr = nullptr;
            if (!context.relativePath.empty() && sepPos)
            {
                //skip directory
                positionStr = (LPCSTR)sepPos;
            }
            else if (context.useFileName)
            {
                positionStr = filename;
            }
            else
            {
                positionStr = ::PathFindFileNameA(filename);
            }

            position = atol(positionStr) - 1;

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
    catch (const std::exception &)
    {
        return SPI_INTERNAL_ERR;
    }
}
int __stdcall GetFile(LPCSTR buf, long len, LPSTR dest, unsigned int flag, FARPROC /*progressCallback*/, long lData)
{
    if (buf == nullptr || dest == nullptr)
    {
        return SPI_INTERNAL_ERR;
    }

    if (flag & 0b000111)
    {
        return SPI_NOT_IMPLEMENT;    // メモリ上のイメージには対応しない
    }

    try
    {
        size_t position = len;
        int ret = SPI_INTERNAL_ERR;

        iterateArchive(buf, [&](Context & context, size_t i, const std::wstring & parent, const WIN32_FIND_DATA & fad)
        {
			if (i > UINT32_MAX)
				return Action::Break;

			DWORD index = static_cast<UINT32>(i);
            if (index != position)
            {
                return Action::Continue;
            }

            WCHAR path[MAX_PATH];
            ::PathCombine(path, parent.c_str(), fad.cFileName);

            if (flag & 0b000011100000000)   // メモリ上のイメージ
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
                if (context.useFileName)
                {
                    swprintf_s(newPath, L"%S\\%s", dest, fad.cFileName);
                }
                else
                {
                    LPCWSTR ext = ::PathFindExtension(fad.cFileName);
                    swprintf_s(newPath, L"%S\\%09u%s", dest, index + 1, ext ? ext : L"");
                }
                ret = ::CopyFile(path, newPath, FALSE) ? SPI_SUCCESS : SPI_FILE_READ_ERR;
            }

            return Action::Break;
        });

        return ret;
    }
    catch (const std::exception &)
    {
        return SPI_INTERNAL_ERR;
    }

    return 0;
}

HMODULE g_hModule = nullptr;

BOOL APIENTRY DllMain(HMODULE hModule,
                      DWORD  ul_reason_for_call,
                      LPVOID /*lpReserved*/
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        g_hModule = hModule;
        break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }

    return TRUE;
}

