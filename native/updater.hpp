#pragma once
#include <cstdint>
#include <string>

inline constexpr wchar_t APP_VERSION[]=L"0.4.0";

struct ReleaseInfo {
  std::wstring tag;
  std::wstring downloadUrl;
  std::string sha256;
  std::uint64_t size=0;
};

bool isNewerVersion(const std::wstring& candidate,const std::wstring& current);
bool parseReleaseInfo(const std::string& json,ReleaseInfo& release);
bool calculateSha256File(const std::wstring& path,std::string& digest);
bool checkForUpdate(ReleaseInfo& release);
bool downloadUpdate(const ReleaseInfo& release,const std::wstring& programPath,
                    std::wstring& error);
bool launchPendingUpdate(const std::wstring& programPath);
int runUpdateHelper(const std::wstring& programPath,const std::wstring& targetPath,
                    unsigned long parentProcessId);
void cleanupUpdateFiles(const std::wstring& programPath);
