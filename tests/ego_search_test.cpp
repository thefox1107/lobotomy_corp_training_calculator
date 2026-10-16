#include "../native/ego_search.hpp"
#include <cassert>
#include <string>

static const Abnormality& byId(const wchar_t* id){
  for(const auto& abnormality:CATALOG)
    if(std::wstring(abnormality.id)==id)return abnormality;
  assert(false&&"missing test abnormality");
  return CATALOG[0];
}

int main(){
  const auto& scorched=byId(L"F-01-02");
  assert(matchesEgoSearch(scorched,L"焦化"));
  assert(matchesEgoSearch(scorched,L"jiao hua"));
  assert(matchesEgoSearch(scorched,L"jhsn"));
  assert(matchesEgoSearch(scorched,L"zmhczg"));
  assert(matchesEgoSearch(scorched,L"终末火柴"));
  assert(!matchesEgoSearch(scorched,L"忏悔"));
  assert(matchesEgoSearch(byId(L"O-03-03"),L"chanhui"));
  assert(matchesEgoSearch(byId(L"O-01-04"),L"zengwunvwang"));
  assert(matchesEgoSearch(byId(L"T-01-31"),L"dacapo"));
  assert(matchesEgoSearch(scorched,L""));
  return 0;
}
