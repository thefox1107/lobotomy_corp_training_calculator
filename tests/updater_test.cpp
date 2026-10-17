#include "../native/updater.hpp"
#include <windows.h>
#include <cassert>
#include <string>

int main(){
  assert(isNewerVersion(L"v0.4.1",APP_VERSION));
  assert(isNewerVersion(L"v0.5.0",APP_VERSION));
  assert(!isNewerVersion(L"v0.4.0",APP_VERSION));
  assert(!isNewerVersion(L"v0.3.5",APP_VERSION));
  assert(!isNewerVersion(L"v0.3.4",APP_VERSION));
  assert(!isNewerVersion(L"v0.3.3",APP_VERSION));
  assert(!isNewerVersion(L"v0.3.2",APP_VERSION));
  assert(!isNewerVersion(L"v0.3.1",APP_VERSION));
  assert(!isNewerVersion(L"v0.3.0",APP_VERSION));
  assert(!isNewerVersion(L"v0.2.9",APP_VERSION));
  assert(!isNewerVersion(L"v0.4.0-beta",APP_VERSION));
  const std::string digest(64,'a');
  const std::string json="{\"tag_name\":\"v0.4.0\",\"assets\":[{"
    "\"name\":\"LobotomyCorpCal.exe\",\"state\":\"uploaded\","
    "\"browser_download_url\":\"https://github.com/thefox1107/"
    "lobotomy_corp_training_calculator/releases/download/v0.4.0/LobotomyCorpCal.exe\","
    "\"size\":1250000,\"digest\":\"sha256:"+digest+"\"}]}";
  ReleaseInfo release;
  assert(parseReleaseInfo(json,release));
  assert(release.tag==L"v0.4.0");
  assert(release.size==1250000);
  assert(release.sha256==digest);
  auto tampered=json;
  auto position=tampered.find("https://github.com/");
  tampered.replace(position,std::string("https://github.com/").size(),"https://example.com/");
  assert(!parseReleaseInfo(tampered,release));
  tampered=json;
  position=tampered.find("sha256:");
  tampered.replace(position,std::string("sha256:").size(),"sha1:");
  assert(!parseReleaseInfo(tampered,release));
  wchar_t temporaryDirectory[MAX_PATH]{},temporaryFile[MAX_PATH]{};
  assert(GetTempPathW(MAX_PATH,temporaryDirectory));
  assert(GetTempFileNameW(temporaryDirectory,L"lcc",0,temporaryFile));
  HANDLE file=CreateFileW(temporaryFile,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL,nullptr);
  assert(file!=INVALID_HANDLE_VALUE);
  DWORD written=0;
  assert(WriteFile(file,"abc",3,&written,nullptr)&&written==3);
  CloseHandle(file);
  std::string actual;
  assert(calculateSha256File(temporaryFile,actual));
  assert(actual=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  DeleteFileW(temporaryFile);
  return 0;
}
