#include "../native/ego_search.hpp"
#include "../native/ego_recommendation.hpp"
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
  assert(matchesEgoSearch(byId(L"T-06-27"),L"\u566a\u97f3"));
  assert(matchesEgoSearch(byId(L"T-06-27"),L"zaoyin"));
  assert(matchesEgoSearch(byId(L"T-02-71"),L"chenzui"));
  assert(matchesEgoSearch(scorched,L""));
  assert(summarizeEgoRecommendation(byId(L"F-01-02").weaponRecommendation)==
         L"良 【远(15)】");
  assert(summarizeEgoRecommendation(byId(L"F-04-42").weaponRecommendation)==
         L"无饰品：良；有饰品：优 【一般(4)】");
  assert(summarizeEgoRecommendation(byId(L"F-01-57").weaponRecommendation)==
         L"良 【极近(2)】");
  assert(summarizeEgoRecommendation(byId(L"T-03-46").weaponRecommendation)==L"未评级");
  for(const auto& abnormality:CATALOG){
    for(const wchar_t* source:{abnormality.weaponRecommendation,
                               abnormality.suitRecommendation}){
      const auto summary=summarizeEgoRecommendation(source);
      for(const wchar_t* risk:{L"(Z)",L"(T)",L"(H)",L"(W)",L"(A)"})
        assert(summary.find(risk)==std::wstring::npos);
      assert(summary.find(L'\n')==std::wstring::npos);
    }
  }
  return 0;
}
