#include "updater.hpp"
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace {
constexpr wchar_t API_URL[]=L"https://api.github.com/repos/thefox1107/lobotomy_corp_training_calculator/releases/latest";
constexpr wchar_t MIRROR_PREFIX[]=L"https://gh-proxy.org/";
constexpr char ASSET_PREFIX[]="https://github.com/thefox1107/lobotomy_corp_training_calculator/releases/download/";
constexpr char ASSET_NAME[]="LobotomyCorpCal.exe";
constexpr std::uint64_t MAX_DOWNLOAD=150ull*1024*1024;
constexpr size_t MAX_METADATA=4*1024*1024;

struct Json {
  enum class Kind {nullValue,string,number,object,array,other} kind=Kind::nullValue;
  std::string value;
  std::map<std::string,Json> members;
  std::vector<Json> elements;
  const Json& at(const std::string& key) const {
    static const Json missing;
    auto found=members.find(key);
    return found==members.end()?missing:found->second;
  }
};

class JsonParser {
  const std::string& input;
  size_t position=0;
  void space(){while(position<input.size()&&std::isspace((unsigned char)input[position]))position++;}
  bool string(std::string& output){
    if(position>=input.size()||input[position++]!='"')return false;
    while(position<input.size()){
      unsigned char ch=input[position++];
      if(ch=='"')return true;
      if(ch<0x20)return false;
      if(ch!='\\'){output.push_back((char)ch);continue;}
      if(position>=input.size())return false;
      ch=input[position++];
      switch(ch){
        case '"':case '\\':case '/':output.push_back((char)ch);break;
        case 'b':output.push_back('\b');break;
        case 'f':output.push_back('\f');break;
        case 'n':output.push_back('\n');break;
        case 'r':output.push_back('\r');break;
        case 't':output.push_back('\t');break;
        case 'u':{
          if(position+4>input.size())return false;
          unsigned value=0;
          for(int i=0;i<4;i++){
            char digit=input[position++];
            if(!std::isxdigit((unsigned char)digit))return false;
            value=value*16+(digit>='0'&&digit<='9'?digit-'0':
                  (std::tolower((unsigned char)digit)-'a'+10));
          }
          output.push_back(value<0x80?(char)value:'?');
          break;
        }
        default:return false;
      }
    }
    return false;
  }
  bool value(Json& result,int depth){
    if(depth>32)return false;
    space();if(position>=input.size())return false;
    char ch=input[position];
    if(ch=='"'){result.kind=Json::Kind::string;return string(result.value);}
    if(ch=='{'){
      result.kind=Json::Kind::object;position++;space();
      if(position<input.size()&&input[position]=='}'){position++;return true;}
      while(position<input.size()){
        std::string key;Json child;
        if(!string(key))return false;
        space();if(position>=input.size()||input[position++]!=':')return false;
        if(!value(child,depth+1))return false;
        result.members.emplace(std::move(key),std::move(child));
        space();if(position>=input.size())return false;
        char end=input[position++];
        if(end=='}')return true;
        if(end!=',')return false;
        space();
      }
      return false;
    }
    if(ch=='['){
      result.kind=Json::Kind::array;position++;space();
      if(position<input.size()&&input[position]==']'){position++;return true;}
      while(position<input.size()){
        Json child;if(!value(child,depth+1))return false;
        result.elements.push_back(std::move(child));
        space();if(position>=input.size())return false;
        char end=input[position++];
        if(end==']')return true;
        if(end!=',')return false;
      }
      return false;
    }
    if(ch=='-'||(ch>='0'&&ch<='9')){
      result.kind=Json::Kind::number;
      size_t start=position++;
      while(position<input.size()&&
            ((input[position]>='0'&&input[position]<='9')||input[position]=='.'||
             input[position]=='e'||input[position]=='E'||input[position]=='+'||
             input[position]=='-'))position++;
      result.value=input.substr(start,position-start);
      return true;
    }
    for(const char* literal:{"true","false","null"}){
      size_t length=std::char_traits<char>::length(literal);
      if(input.compare(position,length,literal)==0){
        result.kind=Json::Kind::other;position+=length;return true;
      }
    }
    return false;
  }
public:
  explicit JsonParser(const std::string& text):input(text){}
  bool parse(Json& result){return value(result,0)&&(space(),position==input.size());}
};

std::wstring widen(const std::string& value){return std::wstring(value.begin(),value.end());}
std::string narrow(const std::wstring& value){return std::string(value.begin(),value.end());}
bool validSha256(const std::string& digest){
  return digest.size()==64&&std::all_of(digest.begin(),digest.end(),[](unsigned char ch){
    return std::isxdigit(ch)!=0;
  });
}
std::wstring directoryOf(const std::wstring& path){
  size_t slash=path.find_last_of(L"\\/");
  return slash==std::wstring::npos?L"":path.substr(0,slash+1);
}
std::wstring stagePath(const std::wstring& program){
  return directoryOf(program)+L"LobotomyCorpCal.update.exe";
}
std::wstring partPath(const std::wstring& program){
  return directoryOf(program)+L"LobotomyCorpCal.update.download";
}
std::wstring markerPath(const std::wstring& program){
  return directoryOf(program)+L"LobotomyCorpCal.update.pending";
}
bool writeText(const std::wstring& path,const std::string& text){
  HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL,nullptr);
  if(file==INVALID_HANDLE_VALUE)return false;
  DWORD written=0;
  bool okay=WriteFile(file,text.data(),(DWORD)text.size(),&written,nullptr)&&
            written==text.size()&&FlushFileBuffers(file);
  CloseHandle(file);
  return okay;
}
std::string readText(const std::wstring& path){
  HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL,nullptr);
  if(file==INVALID_HANDLE_VALUE)return {};
  char data[256]{};DWORD count=0;
  bool okay=ReadFile(file,data,sizeof(data)-1,&count,nullptr)!=FALSE;
  CloseHandle(file);
  return okay?std::string(data,count):std::string();
}
bool hashFile(const std::wstring& path,std::string& digest){
  BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
  if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
  bool okay=BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0;
  HANDLE file=INVALID_HANDLE_VALUE;
  if(okay)file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,
                          nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
  okay=okay&&file!=INVALID_HANDLE_VALUE;
  std::array<UCHAR,65536> buffer{};
  while(okay){
    DWORD count=0;
    if(!ReadFile(file,buffer.data(),(DWORD)buffer.size(),&count,nullptr)){
      okay=false;break;
    }
    if(!count)break;
    okay=BCryptHashData(hash,buffer.data(),count,0)>=0;
  }
  std::array<UCHAR,32> bytes{};
  if(okay)okay=BCryptFinishHash(hash,bytes.data(),(ULONG)bytes.size(),0)>=0;
  if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);
  if(hash)BCryptDestroyHash(hash);
  BCryptCloseAlgorithmProvider(algorithm,0);
  if(!okay)return false;
  static const char hex[]="0123456789abcdef";
  digest.clear();digest.reserve(64);
  for(UCHAR byte:bytes){digest.push_back(hex[byte>>4]);digest.push_back(hex[byte&15]);}
  return true;
}

struct InternetCloser {void operator()(void* handle)const{if(handle)WinHttpCloseHandle(handle);}};
using Internet=std::unique_ptr<void,InternetCloser>;
bool getHttps(const std::wstring& url,std::uint64_t limit,
              const std::function<bool(const char*,DWORD)>& accept){
  URL_COMPONENTSW pieces{};pieces.dwStructSize=sizeof(pieces);
  pieces.dwHostNameLength=(DWORD)-1;pieces.dwUrlPathLength=(DWORD)-1;
  pieces.dwExtraInfoLength=(DWORD)-1;
  if(!WinHttpCrackUrl(url.c_str(),0,0,&pieces)||pieces.nScheme!=INTERNET_SCHEME_HTTPS||
     pieces.lpszUserName||pieces.lpszPassword)return false;
  std::wstring host(pieces.lpszHostName,pieces.dwHostNameLength);
  std::wstring resource(pieces.lpszUrlPath,pieces.dwUrlPathLength);
  if(pieces.lpszExtraInfo)resource.append(pieces.lpszExtraInfo,pieces.dwExtraInfoLength);
  Internet session(WinHttpOpen(L"LobotomyCorpCal/0.3.0",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                               WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
  if(!session)session.reset(WinHttpOpen(L"LobotomyCorpCal/0.3.0",
                                       WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                       WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
  if(!session)return false;
  WinHttpSetTimeouts(session.get(),5000,5000,10000,10000);
  Internet connection(WinHttpConnect(session.get(),host.c_str(),pieces.nPort,0));
  if(!connection)return false;
  Internet request(WinHttpOpenRequest(connection.get(),L"GET",resource.c_str(),nullptr,
                                      WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,
                                      WINHTTP_FLAG_SECURE));
  if(!request)return false;
  DWORD policy=WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
  WinHttpSetOption(request.get(),WINHTTP_OPTION_REDIRECT_POLICY,&policy,sizeof(policy));
  const wchar_t* headers=L"Accept: application/vnd.github+json\r\n";
  if(!WinHttpSendRequest(request.get(),headers,(DWORD)-1,WINHTTP_NO_REQUEST_DATA,0,0,0)||
     !WinHttpReceiveResponse(request.get(),nullptr))return false;
  DWORD status=0,length=sizeof(status);
  if(!WinHttpQueryHeaders(request.get(),WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
                          WINHTTP_HEADER_NAME_BY_INDEX,&status,&length,WINHTTP_NO_HEADER_INDEX)||
     status!=200)return false;
  std::array<char,65536> buffer{};std::uint64_t total=0;
  while(true){
    DWORD count=0;
    if(!WinHttpReadData(request.get(),buffer.data(),(DWORD)buffer.size(),&count))return false;
    if(!count)return true;
    total+=count;
    if(total>limit||!accept(buffer.data(),count))return false;
  }
}
bool downloadFrom(const std::wstring& url,const std::wstring& path,
                  const ReleaseInfo& release){
  HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL,nullptr);
  if(file==INVALID_HANDLE_VALUE)return false;
  std::uint64_t received=0;
  bool okay=getHttps(url,MAX_DOWNLOAD,[&](const char* data,DWORD count){
    DWORD written=0;
    if(!WriteFile(file,data,count,&written,nullptr)||written!=count)return false;
    received+=count;return true;
  });
  if(okay)okay=FlushFileBuffers(file)!=FALSE;
  CloseHandle(file);
  if(!okay||received!=release.size)return false;
  std::string hash;
  if(!hashFile(path,hash))return false;
  std::transform(hash.begin(),hash.end(),hash.begin(),[](unsigned char ch){return std::tolower(ch);});
  return hash==release.sha256;
}
std::array<unsigned long long,3> versionParts(const std::wstring& text,bool& valid){
  std::array<unsigned long long,3> parts{};valid=false;
  size_t pos=!text.empty()&&(text[0]==L'v'||text[0]==L'V')?1:0;
  for(int i=0;i<3;i++){
    if(pos>=text.size()||text[pos]<L'0'||text[pos]>L'9')return parts;
    while(pos<text.size()&&text[pos]>=L'0'&&text[pos]<=L'9'){
      parts[i]=parts[i]*10+(text[pos++]-L'0');
      if(parts[i]>1000000)return parts;
    }
    if(i<2){if(pos>=text.size()||text[pos++]!=L'.')return parts;}
  }
  valid=pos==text.size();return parts;
}
}

bool isNewerVersion(const std::wstring& candidate,const std::wstring& current){
  bool a=false,b=false;
  auto next=versionParts(candidate,a),installed=versionParts(current,b);
  return a&&b&&next>installed;
}
bool calculateSha256File(const std::wstring& path,std::string& digest){
  return hashFile(path,digest);
}
bool parseReleaseInfo(const std::string& json,ReleaseInfo& release){
  if(json.size()>MAX_METADATA)return false;
  Json root;if(!JsonParser(json).parse(root)||root.kind!=Json::Kind::object)return false;
  const Json& tag=root.at("tag_name"),&assets=root.at("assets");
  if(tag.kind!=Json::Kind::string||assets.kind!=Json::Kind::array)return false;
  bool valid=false;versionParts(widen(tag.value),valid);if(!valid)return false;
  const std::string expected=std::string(ASSET_PREFIX)+tag.value+"/"+ASSET_NAME;
  for(const Json& asset:assets.elements){
    if(asset.at("name").value!=ASSET_NAME||asset.at("state").value!="uploaded"||
       asset.at("browser_download_url").value!=expected)continue;
    const std::string& digest=asset.at("digest").value;
    const std::string& size=asset.at("size").value;
    if(digest.rfind("sha256:",0)!=0||!validSha256(digest.substr(7))||size.empty()||
       !std::all_of(size.begin(),size.end(),[](unsigned char ch){return std::isdigit(ch)!=0;}))
      return false;
    std::uint64_t bytes=std::strtoull(size.c_str(),nullptr,10);
    if(bytes<1024||bytes>MAX_DOWNLOAD)return false;
    release={widen(tag.value),widen(expected),digest.substr(7),bytes};
    std::transform(release.sha256.begin(),release.sha256.end(),release.sha256.begin(),
                   [](unsigned char ch){return std::tolower(ch);});
    return true;
  }
  return false;
}
bool checkForUpdate(ReleaseInfo& release){
  for(const std::wstring& url:{std::wstring(API_URL),std::wstring(MIRROR_PREFIX)+API_URL}){
    std::string data;
    bool fetched=getHttps(url,MAX_METADATA,[&](const char* bytes,DWORD count){
      data.append(bytes,count);return true;
    });
    if(fetched&&parseReleaseInfo(data,release))
      return isNewerVersion(release.tag,APP_VERSION);
  }
  return false;
}
bool downloadUpdate(const ReleaseInfo& release,const std::wstring& programPath,
                    std::wstring& error){
  const auto part=partPath(programPath),stage=stagePath(programPath);
  DeleteFileW(part.c_str());
  for(const std::wstring& url:{release.downloadUrl,std::wstring(MIRROR_PREFIX)+release.downloadUrl}){
    if(downloadFrom(url,part,release)){
      if(!MoveFileExW(part.c_str(),stage.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        error=L"无法保存更新文件，请确认程序所在文件夹可写。";return false;
      }
      const std::string marker=narrow(release.tag)+"\n"+release.sha256+"\n";
      if(!writeText(markerPath(programPath),marker)){
        DeleteFileW(stage.c_str());
        error=L"无法保存更新状态，请确认程序所在文件夹可写。";return false;
      }
      return true;
    }
    DeleteFileW(part.c_str());
  }
  error=L"GitHub 与国内镜像均未能完成下载，或文件校验失败。";
  return false;
}
bool launchPendingUpdate(const std::wstring& programPath){
  const auto marker=markerPath(programPath),stage=stagePath(programPath);
  if(GetFileAttributesW(marker.c_str())==INVALID_FILE_ATTRIBUTES)return false;
  const auto text=readText(marker);size_t newline=text.find('\n');
  const auto tag=newline==std::string::npos?std::string():text.substr(0,newline);
  const auto digest=newline==std::string::npos?std::string():
                    text.substr(newline+1,text.find('\n',newline+1)-newline-1);
  std::string actual;
  if(!isNewerVersion(widen(tag),APP_VERSION)||!validSha256(digest)||
     !hashFile(stage,actual)||actual!=digest){
    DeleteFileW(marker.c_str());DeleteFileW(stage.c_str());return false;
  }
  std::wstring command=L"\""+stage+L"\" --apply-update \""+programPath+L"\" "+
                       std::to_wstring(GetCurrentProcessId());
  STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
  if(!CreateProcessW(stage.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,
                     directoryOf(programPath).c_str(),&startup,&process)){
    MessageBoxW(nullptr,L"已下载更新，但无法启动安装步骤。请确认程序文件夹可写。",
                L"更新失败",MB_OK|MB_ICONERROR);
    return false;
  }
  CloseHandle(process.hThread);CloseHandle(process.hProcess);
  return true;
}
int runUpdateHelper(const std::wstring& programPath,const std::wstring& targetPath,
                    unsigned long parentProcessId){
  if(directoryOf(programPath)!=directoryOf(targetPath)||
     programPath!=stagePath(targetPath))return 1;
  HANDLE parent=OpenProcess(SYNCHRONIZE,FALSE,parentProcessId);
  if(parent){DWORD result=WaitForSingleObject(parent,30000);CloseHandle(parent);
    if(result!=WAIT_OBJECT_0)return 2;}
  const std::wstring replacement=targetPath+L".replacement";
  if(!CopyFileW(programPath.c_str(),replacement.c_str(),FALSE)){
    MessageBoxW(nullptr,L"无法写入更新文件，请确认程序所在文件夹可写。",
                L"更新失败",MB_OK|MB_ICONERROR);
    return 3;
  }
  const std::string marker=readText(markerPath(targetPath));
  size_t newline=marker.find('\n');
  const std::string expected=newline==std::string::npos?std::string():
    marker.substr(newline+1,marker.find('\n',newline+1)-newline-1);
  std::string actual;
  if(!validSha256(expected)||!hashFile(replacement,actual)||actual!=expected){
    DeleteFileW(replacement.c_str());
    MessageBoxW(nullptr,L"更新文件校验失败，旧版程序未被修改。",
                L"更新失败",MB_OK|MB_ICONERROR);
    return 4;
  }
  bool replaced=false;
  for(int attempt=0;attempt<40;attempt++){
    if(MoveFileExW(replacement.c_str(),targetPath.c_str(),
                   MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
      replaced=true;break;
    }
    Sleep(500);
  }
  if(!replaced){
    DeleteFileW(replacement.c_str());
    MessageBoxW(nullptr,L"无法替换旧版程序。请关闭其他正在运行的实例后重试。",
                L"更新失败",MB_OK|MB_ICONERROR);
    return 5;
  }
  DeleteFileW(markerPath(targetPath).c_str());
  std::wstring command=L"\""+targetPath+L"\"";
  STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
  if(CreateProcessW(targetPath.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,
                    directoryOf(targetPath).c_str(),&startup,&process)){
    CloseHandle(process.hThread);CloseHandle(process.hProcess);
    return 0;
  }
  MessageBoxW(nullptr,L"更新文件已写入，请手动重新打开程序。",L"更新完成",MB_OK);
  return 0;
}
void cleanupUpdateFiles(const std::wstring& programPath){
  if(GetFileAttributesW(markerPath(programPath).c_str())!=INVALID_FILE_ATTRIBUTES)return;
  DeleteFileW(partPath(programPath).c_str());
  DeleteFileW(stagePath(programPath).c_str());
}
