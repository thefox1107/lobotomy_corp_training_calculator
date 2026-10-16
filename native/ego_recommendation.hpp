#pragma once
#include <cwchar>
#include <initializer_list>
#include <string>

inline std::wstring summarizeEgoRecommendation(const wchar_t* source){
  if(!source||!*source||source[0]==L'-'&&source[1]==0)return L"—";
  const std::wstring original(source);
  std::wstring result;
  size_t lineStart=0;
  while(lineStart<original.size()){
    size_t lineEnd=original.find(L'\n',lineStart);
    if(lineEnd==std::wstring::npos)lineEnd=original.size();
    std::wstring line=original.substr(lineStart,lineEnd-lineStart);
    size_t gradeAt=std::wstring::npos;
    const wchar_t* grade=nullptr;
    for(size_t pos=0;pos<line.size()&&!grade;pos++){
      for(const wchar_t* candidate:{L"极优",L"极差",L"优",L"良",L"差"}){
        if(line.compare(pos,wcslen(candidate),candidate)==0){
          gradeAt=pos;grade=candidate;break;
        }
      }
    }
    if(grade){
      std::wstring entry;
      size_t colon=line.rfind(L'：',gradeAt);
      if(colon!=std::wstring::npos)entry=line.substr(0,colon+1);
      entry+=grade;
      size_t distanceStart=line.find(L'【',gradeAt+wcslen(grade));
      size_t distanceEnd=distanceStart==std::wstring::npos?std::wstring::npos:
                         line.find(L'】',distanceStart+1);
      if(distanceEnd!=std::wstring::npos){
        entry+=L" ";
        entry+=line.substr(distanceStart,distanceEnd-distanceStart+1);
      }
      if(!result.empty())result+=L"；";
      result+=entry;
    }
    lineStart=lineEnd+1;
  }
  return result.empty()?L"未评级":result;
}
