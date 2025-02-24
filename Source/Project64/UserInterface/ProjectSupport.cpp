#include "stdafx.h"

#include <Common/md5.h>
#include <Wininet.h>
#include <time.h>
#include <windows.h>

#pragma comment(lib, "Wininet.lib")

CProjectSupport::CProjectSupport() :
    m_SupportInfo({0})
{
    LoadSupportInfo();
}

bool CProjectSupport::RequestCode(const char * Email)
{
    return true;
}

bool CProjectSupport::ValidateCode(const char * Code)
{
    return true;
}

std::string CProjectSupport::GenerateMachineID(void)
{
    wchar_t ComputerName[256];
    DWORD Length = sizeof(ComputerName) / sizeof(ComputerName[0]);
    GetComputerName(ComputerName, &Length);

    wchar_t SysPath[MAX_PATH] = {0}, VolumePath[MAX_PATH] = {0};
    GetSystemDirectory(SysPath, sizeof(SysPath) / sizeof(SysPath[0]));

    GetVolumePathName(SysPath, VolumePath, sizeof(VolumePath) / sizeof(VolumePath[0]));

    DWORD SerialNumber = 0;
    GetVolumeInformation(VolumePath, nullptr, NULL, &SerialNumber, nullptr, nullptr, nullptr, NULL);

    wchar_t MachineGuid[200] = {0};
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS)
    {
        DWORD Type, dwDataSize = sizeof(MachineGuid);
        RegQueryValueEx(hKey, L"MachineGuid", nullptr, &Type, (LPBYTE)MachineGuid, &dwDataSize);
        RegCloseKey(hKey);
    }

    stdstr_f Machine("%s.%ud.%s", stdstr().FromUTF16(ComputerName).c_str(), SerialNumber, stdstr().FromUTF16(MachineGuid).c_str());
    return std::string(MD5((const unsigned char *)Machine.c_str(), (uint32_t)((UINT_PTR)Machine.size())).hex_digest());
}

void CProjectSupport::IncrementRunCount()
{
    return;
}

bool CProjectSupport::ShowSupportWindow()
{
    return false;
}

bool CProjectSupport::PerformRequest(const wchar_t * Url, const std::string & PostData, DWORD & StatusCode, std::vector<std::string> & Headers)
{
    StatusCode = 0;
    Headers.clear();

    HINTERNET hSession = InternetOpen(L"Project64", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (hSession == nullptr)
    {
        return false;
    }

    HINTERNET hConnect = InternetConnect(hSession, L"www.pj64-emu.com", INTERNET_DEFAULT_HTTPS_PORT, L"", L"", INTERNET_SERVICE_HTTP, 0, 0);
    if (hConnect == nullptr)
    {
        return false;
    }
    LPCWSTR lpszAcceptTypes[] = {
        L"text/*",
        nullptr,
    };
    HINTERNET hRequest = HttpOpenRequest(hConnect, L"POST", Url, nullptr, nullptr, lpszAcceptTypes, INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_AUTO_REDIRECT | INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_NO_CACHE_WRITE, (LPARAM)0);
    if (hRequest == nullptr)
    {
        InternetCloseHandle(hRequest);
        return false;
    }
    wchar_t hdheaders[] = _T("Content-Type: application/x-www-form-urlencoded\r\n");
    BOOL Success = HttpSendRequest(hRequest, hdheaders, (uint32_t)((UINT_PTR)_tcslen(hdheaders)), (LPVOID)PostData.c_str(), (uint32_t)((UINT_PTR)PostData.length()));
    if (!Success)
    {
        InternetCloseHandle(hRequest);
        return false;
    }

    DWORD StatusCodeSize = sizeof(StatusCode);
    if (!HttpQueryInfo(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &StatusCode, &StatusCodeSize, nullptr))
    {
        InternetCloseHandle(hRequest);
        return false;
    }

    DWORD dwSize = 0;
    if (!HttpQueryInfo(hRequest, HTTP_QUERY_RAW_HEADERS_CRLF, nullptr, &dwSize, nullptr) && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
    {
        std::vector<uint8_t> RawHeaderData(dwSize);
        if (!HttpQueryInfo(hRequest, HTTP_QUERY_RAW_HEADERS_CRLF, RawHeaderData.data(), &dwSize, nullptr))
        {
            InternetCloseHandle(hRequest);
            return false;
        }
        strvector RawHeader = stdstr().FromUTF16((wchar_t *)RawHeaderData.data()).Tokenize("\n");
        for (size_t i = 0, n = RawHeader.size(); i < n; i++)
        {
            RawHeader[i].Replace("\r", "");
            RawHeader[i].Trim();
            if (RawHeader[i].length() > 0)
            {
                Headers.push_back(RawHeader[i]);
            }
        }
    }
    return true;
}

void CProjectSupport::SaveSupportInfo(void)
{
    std::string hash = MD5((const unsigned char *)&m_SupportInfo, sizeof(m_SupportInfo)).hex_digest();

    std::vector<uint8_t> InData(sizeof(m_SupportInfo) + hash.length());
    memcpy(InData.data(), (const unsigned char *)&m_SupportInfo, sizeof(m_SupportInfo));
    memcpy(InData.data() + sizeof(m_SupportInfo), hash.data(), hash.length());
    std::vector<uint8_t> OutData(InData.size());

    z_stream defstream;
    defstream.zalloc = Z_NULL;
    defstream.zfree = Z_NULL;
    defstream.opaque = Z_NULL;
    defstream.avail_in = (uInt)InData.size();
    defstream.next_in = (Bytef *)InData.data();
    defstream.avail_out = (uInt)OutData.size();
    defstream.next_out = (Bytef *)OutData.data();

    deflateInit(&defstream, Z_BEST_COMPRESSION);
    deflate(&defstream, Z_FINISH);
    deflateEnd(&defstream);

    OutData.resize(defstream.total_out);

    for (size_t i = 0, n = OutData.size(); i < n; i++)
    {
        OutData[i] ^= 0xAA;
    }

    HKEY hKeyResults = 0;
    DWORD Disposition = 0;
    long lResult = RegCreateKeyEx(HKEY_CURRENT_USER, L"SOFTWARE\\Project64", 0, L"", REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr, &hKeyResults, &Disposition);
    if (lResult == ERROR_SUCCESS)
    {
        RegSetValueEx(hKeyResults, L"user", 0, REG_BINARY, (BYTE *)OutData.data(), (uint32_t)((UINT_PTR)OutData.size()));
        RegCloseKey(hKeyResults);
    }
}

void CProjectSupport::LoadSupportInfo(void)
{
    std::string MachineID = GenerateMachineID();
    std::vector<uint8_t> InData;

    HKEY hKeyResults = 0;
    long lResult = RegOpenKeyEx(HKEY_CURRENT_USER, L"SOFTWARE\\Project64", 0, KEY_READ, &hKeyResults);
    if (lResult == ERROR_SUCCESS)
    {
        DWORD DataSize = 0;
        if (RegQueryValueEx(hKeyResults, L"user", nullptr, nullptr, nullptr, &DataSize) == ERROR_SUCCESS)
        {
            InData.resize(DataSize);
            if (RegQueryValueEx(hKeyResults, L"user", nullptr, nullptr, InData.data(), &DataSize) != ERROR_SUCCESS)
            {
                InData.clear();
            }
        }
    }

    if (hKeyResults != nullptr)
    {
        RegCloseKey(hKeyResults);
        hKeyResults = nullptr;
    }

    std::vector<uint8_t> OutData;
    if (InData.size() > 0)
    {
        for (size_t i = 0, n = InData.size(); i < n; i++)
        {
            InData[i] ^= 0xAA;
        }
        OutData.resize(sizeof(m_SupportInfo) + 100);
        uLongf DestLen = (uLongf)((UINT_PTR)OutData.size());
        if (uncompress(OutData.data(), &DestLen, InData.data(), (uint32_t)((UINT_PTR)InData.size())) >= 0)
        {
            OutData.resize(DestLen);
        }
        else
        {
            OutData.clear();
        }
    }

    if (OutData.size() == sizeof(SupportInfo) + 32)
    {
        SupportInfo * Info = (SupportInfo *)OutData.data();
        const char * CurrentHash = (const char *)(OutData.data() + sizeof(SupportInfo));
        std::string hash = MD5((const unsigned char *)Info, sizeof(SupportInfo)).hex_digest();
        if (strcmp(hash.c_str(), CurrentHash) == 0 && strcmp(Info->MachineID, MachineID.c_str()) == 0)
        {
            memcpy(&m_SupportInfo, Info, sizeof(SupportInfo));
        }
    }
    strcpy(m_SupportInfo.MachineID, MachineID.c_str());
}
