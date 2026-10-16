#pragma once
#include <cwctype>
#include <string>
#include "catalog.hpp"

inline std::wstring normalizeEgoSearch(const std::wstring& input){
  std::wstring result;
  for(wchar_t ch:input){
    if(ch==L'ü'||ch==L'Ü')ch=L'v';
    ch=std::towlower(ch);
    if((ch>=L'0'&&ch<=L'9')||(ch>=L'a'&&ch<=L'z')||
       (ch>=0x3400&&ch<=0x9fff))result.push_back(ch);
  }
  return result;
}

inline bool fuzzyContains(const std::wstring& field,const std::wstring& query){
  if(field.find(query)!=std::wstring::npos)return true;
  size_t at=0;
  for(wchar_t ch:field)if(at<query.size()&&ch==query[at])++at;
  return at==query.size();
}

inline bool matchesEgoSearch(const Abnormality& abnormality,const std::wstring& query){
  const auto needle=normalizeEgoSearch(query);
  if(needle.empty())return true;
  for(const wchar_t* value:{abnormality.name,abnormality.egoWeaponName,abnormality.egoSuitName,
                            abnormality.abnormalityPinyin,abnormality.abnormalityInitials,
                            abnormality.weaponPinyin,abnormality.weaponInitials})
    if(fuzzyContains(normalizeEgoSearch(value),needle))return true;
  return false;
}
