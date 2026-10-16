#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <string>
#include <thread>
#include <memory>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdlib>
#include <vector>
#include <array>
#include <cwchar>
#include "catalog.hpp"
#include "core.hpp"
#include "ego_search.hpp"
#include "ego_recommendation.hpp"
#include "updater.hpp"

static constexpr UINT WM_UPDATE_FOUND=WM_APP+51;
static constexpr UINT WM_UPDATE_DOWNLOADED=WM_APP+52;
struct UpdateDownloadResult {bool success;std::wstring error;};
static std::wstring programPath;
static bool updateDownloading=false,updateReady=false;
static int updateCleanupTicks=0;

static constexpr COLORREF BG=RGB(242,244,243), CARD=RGB(255,255,255);
static constexpr COLORREF DARK=RGB(28,38,43), INK=RGB(36,47,50);
static constexpr COLORREF MUTED=RGB(103,115,118), BORDER=RGB(220,226,224);
static constexpr COLORREF RED=RGB(177,66,59), SOFT_RED=RGB(250,238,236);
static constexpr COLORREF GREEN=RGB(55,122,93), SOFT_GREEN=RGB(232,245,237), ROW=RGB(247,249,248);
static constexpr COLORREF SHADOW=RGB(228,232,230), PANEL=RGB(38,50,55);
static constexpr COLORREF FIELD=RGB(250,251,250), HEADER_BG=RGB(242,246,244);
static constexpr COLORREF WORK_COLORS[]={RED,RGB(112,120,123),RGB(124,76,157),RGB(48,112,177)};
static constexpr COLORREF WORK_BACKGROUNDS[]={SOFT_RED,RGB(241,243,243),RGB(245,239,249),RGB(235,243,251)};
static constexpr COLORREF RISK_COLORS[]={
  RGB(46,128,66),   // ZAYIN: green
  RGB(48,112,177),  // TETH: blue
  RGB(255,255,0),  // HE: yellow
  RGB(124,76,157),  // WAW: purple
  RGB(175,58,63)    // ALEPH: red
};
static HWND win,abnoBox,nameEdit,riskText,capBox,stats[4],targets[4],done[4];
static HWND techBox,residentBox,clerksBox,rates[4][5],doneMinus[4],donePlus[4],virtueLabels[4];
static HWND abnoLabel,capLabel,attributeHeaders[4],residentLabel,clerksLabel;
static HWND tabs;
static HWND simpleList,containedList,availableCombo,containAddButton,containRemoveButton;
static int hoveredSimpleRow=-1;
static bool trackingSimpleMouse=false;
static int hoveredContainedRow=-1;
static bool trackingContainedMouse=false;
static int hoveredRecommendationRow=-1;
static bool trackingRecommendationMouse=false;
static int hoveredEgoRow=-1;
static bool trackingEgoMouse=false;
static HWND recommendationList,recommendationDetail,egoList,egoButtons[4],egoSearch,egoSearchCount;
static constexpr int PAGE_COUNT=6,HELP_PAGE=5;
static std::array<std::vector<HWND>,PAGE_COUNT> pageControls;
struct ControlFrame { int page; HWND item; int x,y,w,h; };
struct ControlLayout {
  HWND item;
  int page,x,y,w,h,id;
  bool combo,list,framed;
  bool placed=false;
  int lastX=0,lastY=0,lastW=0,lastH=0;
  int clipLeft=0,clipTop=0,clipRight=0,clipBottom=0;
};
static std::vector<ControlFrame> controlFrames;
static std::vector<ControlLayout> controlLayouts;
static std::vector<bool> contained;
static std::array<int,CATALOG_SIZE> egoWeapons{},egoSuits{};
static int currentPage=HELP_PAGE;
static int recommendationSortColumn=0;
static bool recommendationDescending=false;
static std::vector<int> recommendationOrder;
static std::vector<int> egoOrder;
static int egoSortColumn=0;
static bool egoSortDescending=false;
static std::wstring containmentMessage;
static constexpr int SIDEBAR_WIDTH=190;
static constexpr int BASE_WIDTH=SIDEBAR_WIDTH+1200,BASE_HEIGHT=770;
static constexpr int MIN_WIDTH=900,MIN_HEIGHT=500;
static constexpr int DETAIL_SCROLL_WIDTH=1180,DETAIL_SCROLL_HEIGHT=830;
static constexpr int DETAIL_LAYOUT_HEIGHT=850;
static constexpr DWORD WINDOW_STYLE=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|
  WS_MINIMIZEBOX|WS_MAXIMIZEBOX|WS_THICKFRAME|WS_CLIPCHILDREN|WS_HSCROLL|WS_VSCROLL;
static constexpr int DETAIL_SHIFT=105;
static constexpr int TAB_X=55,TAB_Y=101,TAB_WIDTH=96,TAB_GAP=8,TAB_HEIGHT=34;
static constexpr int VISIBLE_TABS=9;
static int firstVisibleTab=0;
static int scrollX=0,scrollY=0;
static bool layingOut=false;
static bool resizingWindow=false,resizePending=false;
static int extraWidth(){
  RECT area{};if(win)GetClientRect(win,&area);
  int extra=(int)area.right-BASE_WIDTH;
  return currentPage==1?std::max(0,extra):extra;
}
static int extraHeight(){
  RECT area{};if(win)GetClientRect(win,&area);
  return currentPage==1?std::max(0,(int)area.bottom-DETAIL_LAYOUT_HEIGHT):
                        (int)area.bottom-BASE_HEIGHT;
}
static int detailLeftGrowth(){return extraWidth()/2;}
static int detailRightGrowth(){return extraWidth()-detailLeftGrowth();}
static int preferenceCellWidth(int extra){return 82+extra/8;}
static int preferenceCellX(int column,int extra){
  int growth=extra/8;
  return 745+column*100+column*growth+column*(extra-4*growth)/3;
}
static void detailControlBounds(HWND item,int baseX,int baseY,int baseW,int baseH,
                                int& x,int& y,int& w,int& h){
  x=baseX;y=baseY;w=baseW;h=baseH;
  int left=detailLeftGrowth(),right=detailRightGrowth();
  if(baseX>=648){
    x+=left;
    for(int i=0;i<4;i++)for(int j=0;j<5;j++)if(item==rates[i][j]){
      x=left+preferenceCellX(i,right);
      w=preferenceCellWidth(right);
    }
  }else{
    if(item==nameEdit||item==riskText)w+=left/2;
    if(item==abnoLabel||item==capLabel)x+=left/2;
    if(item==abnoBox||item==capBox){x+=left/2;w+=left/2;}
    for(int i=0;i<4;i++){
      if(item==attributeHeaders[i])x+=i*left/4;
      if(item==stats[i]){x+=left/4;w+=left/8;}
      if(item==targets[i]){x+=left/2;w+=left/8;}
      if(item==done[i]){x+=3*left/4;w+=left/8;}
      if(item==doneMinus[i]||item==donePlus[i])x+=left;
    }
    if(item==residentLabel||item==residentBox)x+=left/2;
    if(item==clerksLabel||item==clerksBox)x+=left;
  }
  if(baseY>=547)y+=extraHeight();
}
static HFONT bodyFont,smallFont,titleFont,sectionFont,emphasisFont,headerFont;
static HFONT currentBodyFont(){return bodyFont;}
static HFONT currentHeaderFont(){return headerFont;}
static HBRUSH cardBrush,editBrush;
static std::wstring iniPath,summary,detail;
static PlanInput viewPlan;
static WorkResult viewResults[4];
struct EmployeeState {
  std::wstring name=L"新员工";
  int abnormality=1,capIndex=0,resident=0,clerks=0;
  bool tech=false;
  std::array<double,4> current{20,20,20,20},target{30,30,30,30};
  std::array<int,4> completed{0,0,0,0};
};
static std::vector<EmployeeState> employees;
static int activeEmployee=0,selected=1,best=-1,missing=0;
static bool ready=false,trainingComplete=false;
static const wchar_t* works[]={L"本能",L"洞察",L"沟通",L"压迫"};
static const wchar_t* virtues[]={L"勇气",L"谨慎",L"自律",L"正义"};
static const wchar_t* levels[]={L"ZAYIN",L"TETH",L"HE",L"WAW",L"ALEPH"};
static const double expectedBoxes[]={5,8,12,18,23};
static const wchar_t* pageNames[]={L"简单模式",L"详细模式",L"当前收容",L"收容推荐",L"E.G.O 库存",L"使用说明"};
static constexpr int sidebarPages[]={HELP_PAGE,0,1,2,3,4};

static std::wstring getText(HWND h){wchar_t buf[256]{};GetWindowTextW(h,buf,256);return buf;}
static void setText(HWND h,const std::wstring& s){SetWindowTextW(h,s.c_str());}
static std::wstring fmt(double v,int n=2){std::wostringstream s;s<<std::fixed<<std::setprecision(n)<<v;return s.str();}
static double num(HWND h,double fallback){
  auto s=getText(h);if(s.empty())return fallback;
  wchar_t* end=nullptr;double v=wcstod(s.c_str(),&end);return end==s.c_str()?fallback:v;
}
static int integer(HWND h,int fallback){return (int)num(h,fallback);}
static std::wstring prefSection(){return L"prefs_"+std::wstring(CATALOG[selected].id);}
static std::wstring readIni(const wchar_t* section,const wchar_t* key,const wchar_t* def){
  wchar_t buf[256]{};GetPrivateProfileStringW(section,key,def,buf,256,iniPath.c_str());return buf;
}
static void writeIni(const wchar_t* section,const wchar_t* key,const std::wstring& value){
  WritePrivateProfileStringW(section,key,value.c_str(),iniPath.c_str());
}
static std::wstring employeeSection(int index){return L"employee_"+std::to_wstring(index);}
static EmployeeState loadEmployee(const std::wstring& section){
  EmployeeState e;
  e.name=readIni(section.c_str(),L"name",L"新员工");
  if(e.name.empty())e.name=L"新员工";
  auto id=readIni(section.c_str(),L"abno",L"O-03-03");
  for(int i=0;i<CATALOG_SIZE;i++)if(id==CATALOG[i].id){e.abnormality=i;break;}
  e.capIndex=std::clamp(_wtoi(readIni(section.c_str(),L"cap",L"0").c_str()),0,2);
  e.tech=readIni(section.c_str(),L"tech",L"0")==L"1";
  e.resident=std::clamp(_wtoi(readIni(section.c_str(),L"resident",L"0").c_str()),0,3);
  e.clerks=std::clamp(_wtoi(readIni(section.c_str(),L"clerks",L"0").c_str()),0,3);
  for(int i=0;i<4;i++){
    auto n=std::to_wstring(i);
    e.current[i]=_wtof(readIni(section.c_str(),(L"current"+n).c_str(),L"20").c_str());
    e.target[i]=_wtof(readIni(section.c_str(),(L"target"+n).c_str(),L"30").c_str());
    e.completed[i]=std::clamp(_wtoi(readIni(section.c_str(),(L"done"+n).c_str(),L"0").c_str()),0,10000);
  }
  return e;
}
static void loadEmployees(){
  int count=_wtoi(readIni(L"employees",L"count",L"0").c_str());
  if(count<=0)employees.push_back(loadEmployee(L"plan"));
  else{
    count=std::clamp(count,1,200);
    for(int i=0;i<count;i++)employees.push_back(loadEmployee(employeeSection(i)));
    activeEmployee=std::clamp(_wtoi(readIni(L"employees",L"selected",L"0").c_str()),0,count-1);
  }
}
static void loadCollection(){
  contained.assign(CATALOG_SIZE,false);
  bool initialized=readIni(L"containment",L"initialized",L"0")==L"1";
  if(initialized){
    for(int i=0;i<CATALOG_SIZE;i++)
      contained[i]=readIni(L"containment",CATALOG[i].id,L"0")==L"1";
  }
  for(const auto& e:employees)contained[e.abnormality]=true;
  writeIni(L"containment",L"initialized",L"1");
  for(int i=0;i<CATALOG_SIZE;i++)
    writeIni(L"containment",CATALOG[i].id,contained[i]?L"1":L"0");
  for(int i=0;i<CATALOG_SIZE;i++){
    auto weaponKey=std::wstring(CATALOG[i].id)+L"_weapon";
    auto suitKey=std::wstring(CATALOG[i].id)+L"_suit";
    egoWeapons[i]=std::clamp(_wtoi(readIni(L"ego",weaponKey.c_str(),L"0").c_str()),0,CATALOG[i].egoWeaponMax);
    egoSuits[i]=std::clamp(_wtoi(readIni(L"ego",suitKey.c_str(),L"0").c_str()),0,CATALOG[i].egoSuitMax);
  }
}
static void writeEmployee(int index){
  const auto& e=employees[index];auto section=employeeSection(index);
  writeIni(section.c_str(),L"name",e.name);
  writeIni(section.c_str(),L"abno",CATALOG[e.abnormality].id);
  writeIni(section.c_str(),L"cap",std::to_wstring(e.capIndex));
  writeIni(section.c_str(),L"tech",e.tech?L"1":L"0");
  writeIni(section.c_str(),L"resident",std::to_wstring(e.resident));
  writeIni(section.c_str(),L"clerks",std::to_wstring(e.clerks));
  for(int i=0;i<4;i++){
    auto n=std::to_wstring(i);
    writeIni(section.c_str(),(L"current"+n).c_str(),fmt(e.current[i],4));
    writeIni(section.c_str(),(L"target"+n).c_str(),fmt(e.target[i],4));
    writeIni(section.c_str(),(L"done"+n).c_str(),std::to_wstring(e.completed[i]));
  }
  writeIni(L"employees",L"count",std::to_wstring(employees.size()));
  writeIni(L"employees",L"selected",std::to_wstring(activeEmployee));
}
static void roundControl(HWND item,int diameter){
  RECT bounds{};
  GetWindowRect(item,&bounds);
  int width=bounds.right-bounds.left,height=bounds.bottom-bounds.top;
  if(width<=0||height<=0)return;
  HRGN shape=CreateRoundRectRgn(1,1,width-1,height-1,diameter,diameter);
  if(shape&&!SetWindowRgn(item,shape,TRUE))DeleteObject(shape);
}
static void clipDetailControl(HWND item,bool roundedShape,int diameter){
  RECT bounds{};GetWindowRect(item,&bounds);
  MapWindowPoints(HWND_DESKTOP,win,(POINT*)&bounds,2);
  int width=bounds.right-bounds.left,height=bounds.bottom-bounds.top;
  HRGN shape=roundedShape?
    CreateRoundRectRgn(1,1,width-1,height-1,diameter,diameter):
    CreateRectRgn(0,0,width,height);
  RECT client{};GetClientRect(win,&client);
  int left=std::clamp(SIDEBAR_WIDTH-(int)bounds.left,0,width);
  int top=std::clamp(-(int)bounds.top,0,height);
  int right=std::clamp((int)(client.right-bounds.left),left,width);
  int bottom=std::clamp((int)(client.bottom-bounds.top),top,height);
  HRGN visible=CreateRectRgn(left,top,right,bottom);
  CombineRgn(shape,shape,visible,RGN_AND);
  DeleteObject(visible);
  if(!SetWindowRgn(item,shape,TRUE))DeleteObject(shape);
}
static LRESULT CALLBACK comboSubclass(HWND combo,UINT message,WPARAM wp,LPARAM lp,
                                      UINT_PTR,DWORD_PTR){
  if(message==WM_ERASEBKGND)return 1;
  if(message==WM_PAINT){
    PAINTSTRUCT paint{};HDC dc=BeginPaint(combo,&paint);
    RECT area{};GetClientRect(combo,&area);
    HBRUSH background=CreateSolidBrush(FIELD);
    HPEN border=CreatePen(PS_SOLID,1,GetFocus()==combo?RED:BORDER);
    HGDIOBJ oldBrush=SelectObject(dc,background),oldPen=SelectObject(dc,border);
    RoundRect(dc,1,1,area.right-1,area.bottom-1,12,12);
    SelectObject(dc,oldBrush);SelectObject(dc,oldPen);
    DeleteObject(background);DeleteObject(border);
    wchar_t caption[256]{};
    int selected=(int)SendMessageW(combo,CB_GETCURSEL,0,0);
    if(selected>=0)SendMessageW(combo,CB_GETLBTEXT,selected,(LPARAM)caption);
    SetBkMode(dc,TRANSPARENT);
    SetTextColor(dc,IsWindowEnabled(combo)?INK:MUTED);
    HGDIOBJ oldFont=SelectObject(dc,currentBodyFont());
    RECT textArea{13,1,area.right-31,area.bottom-1};
    DrawTextW(dc,caption,-1,&textArea,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
    SelectObject(dc,oldFont);
    HPEN arrow=CreatePen(PS_SOLID,1,MUTED);oldPen=SelectObject(dc,arrow);
    int x=area.right-17,y=(area.bottom-area.top)/2;
    MoveToEx(dc,x-4,y-2,nullptr);LineTo(dc,x,y+2);LineTo(dc,x+4,y-2);
    SelectObject(dc,oldPen);DeleteObject(arrow);
    EndPaint(combo,&paint);return 0;
  }
  if(message==WM_SETFOCUS||message==WM_KILLFOCUS||message==WM_ENABLE){
    LRESULT result=DefSubclassProc(combo,message,wp,lp);
    InvalidateRect(combo,nullptr,FALSE);return result;
  }
  if(message==WM_NCDESTROY)RemoveWindowSubclass(combo,comboSubclass,1);
  return DefSubclassProc(combo,message,wp,lp);
}
static HWND pageControl(int page,const wchar_t* cls,const wchar_t* value,DWORD style,int x,int y,int w,int h,int id=0){
  const bool framed=wcscmp(cls,L"EDIT")==0||wcscmp(cls,L"COMBOBOX")==0||
                    wcscmp(cls,WC_LISTVIEWW)==0;
  if(framed)style&=~WS_BORDER;
  if(wcscmp(cls,L"BUTTON")==0&&(style&BS_TYPEMASK)==BS_PUSHBUTTON)
    style=(style&~BS_TYPEMASK)|BS_OWNERDRAW;
  if(wcscmp(cls,L"COMBOBOX")==0&&(style&CBS_DROPDOWNLIST)==CBS_DROPDOWNLIST)
    style|=CBS_OWNERDRAWFIXED|CBS_HASSTRINGS;
  HWND item=CreateWindowExW(0,cls,value,WS_CHILD|WS_VISIBLE|style,x+SIDEBAR_WIDTH,y,w,h,win,
                           (HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);
  SendMessageW(item,WM_SETFONT,(WPARAM)bodyFont,TRUE);
  if(wcscmp(cls,L"EDIT")==0)
    SendMessageW(item,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELPARAM(7,7));
  if(wcscmp(cls,L"COMBOBOX")==0){
    SendMessageW(item,CB_SETITEMHEIGHT,(WPARAM)-1,28);
    SendMessageW(item,CB_SETITEMHEIGHT,0,27);
  }
  if(wcscmp(cls,L"EDIT")==0||wcscmp(cls,L"COMBOBOX")==0)
    roundControl(item,12);
  else if(wcscmp(cls,WC_LISTVIEWW)==0)
    roundControl(item,16);
  RECT actual{};GetWindowRect(item,&actual);
  if(framed)controlFrames.push_back({page,item,x,y,w,actual.bottom-actual.top});
  controlLayouts.push_back({item,page,x,y,w,h,id,wcscmp(cls,L"COMBOBOX")==0,
                            wcscmp(cls,WC_LISTVIEWW)==0,framed});
  if(wcscmp(cls,L"COMBOBOX")==0)SetWindowSubclass(item,comboSubclass,1,0);
  pageControls[page].push_back(item);return item;
}
static HWND control(const wchar_t* cls,const wchar_t* value,DWORD style,int x,int y,int w,int h,int id=0){
  return pageControl(1,cls,value,style,x,y>=268?y-DETAIL_SHIFT:y,w,h,id);
}
static HWND label(const wchar_t* value,int x,int y,int w,int h=22){
  return control(L"STATIC",value,0,x,y,w,h);
}
static HWND edit(int x,int y,int w,int id,const wchar_t* value=L""){
  return control(L"EDIT",value,WS_BORDER|ES_AUTOHSCROLL,x,y,w,29,id);
}
static void fillRates(){
  ready=false;
  bool known=CATALOG[selected].rates[0][0]!=-101;
  auto section=prefSection();
  for(int i=0;i<4;i++)for(int j=0;j<5;j++){
    if(known)setText(rates[i][j],std::to_wstring(CATALOG[selected].rates[i][j]));
    else{
      auto key=L"r"+std::to_wstring(i)+L"_"+std::to_wstring(j);
      setText(rates[i][j],readIni(section.c_str(),key.c_str(),L""));
    }
    EnableWindow(rates[i][j],!known&&CATALOG[selected].regularWork);
  }
  ready=true;
}
static PlanInput readPlan(){
  PlanInput p;p.level=CATALOG[selected].level;
  static constexpr int caps[]={100,120,130};
  p.cap=caps[std::clamp((int)SendMessageW(capBox,CB_GETCURSEL,0,0),0,2)];
  p.tech=SendMessageW(techBox,BM_GETCHECK,0,0)==BST_CHECKED;
  p.resident=(int)SendMessageW(residentBox,CB_GETCURSEL,0,0);
  p.clerks=(int)SendMessageW(clerksBox,CB_GETCURSEL,0,0);
  p.boxes=expectedBoxes[p.level];
  p.hp=1.;p.sp=1.;
  for(int i=0;i<4;i++){
    p.current[i]=std::clamp(num(stats[i],20.),1.,(double)p.cap);
    p.target[i]=std::clamp(num(targets[i],30.),p.current[i],(double)p.cap);
    p.completed[i]=std::clamp(integer(done[i],0),0,10000);
    for(int j=0;j<5;j++){
      auto s=getText(rates[i][j]);
      p.rates[i][j]=s.empty()?-101:std::clamp(integer(rates[i][j],-101),-100,100);
    }
  }
  return p;
}
static void updateOutput(){
  if(!ready)return;
  viewPlan=readPlan();best=-1;missing=0;trainingComplete=false;
  if(!CATALOG[selected].regularWork){
    summary=L"此对象没有常规工作";
    detail=L"工具类或无法收容的对象不参与训练推荐。";
  }else{
    int bestRate=-1;bool allDone=true;
    for(int i=0;i<4;i++){
      viewResults[i]=simulate(viewPlan,i);
      auto r=viewResults[i];
      if(r.rate==-101){missing++;allDone=false;}
      if(r.remaining>0||!r.reached)allDone=false;
      if(r.rate>0&&r.remaining>0&&r.rate>bestRate){best=i;bestRate=r.rate;}
    }
    if(allDone){
      trainingComplete=true;best=-1;
      summary=L"训练计划已完成";
      detail=L"四项属性的预计剩余次数均为 0。";
    }else if(best>=0){
      summary=std::wstring(L"推荐工作  ·  ")+works[best];
      detail=L"基础偏好 "+std::to_wstring(viewResults[best].rate)+L"%   ·   还需约 "
            +std::to_wstring(viewResults[best].remaining)+L" 次";
    }else if(missing){
      summary=L"需要补录工作偏好";
      detail=L"请按 Wiki 数值填写空白偏好后再查看推荐。";
    }else{
      summary=L"当前没有可推荐工作";
      detail=L"训练目标已达到，或剩余工作的基础偏好不大于 0%。";
    }
  }
  InvalidateRect(win,nullptr,FALSE);
}
static std::wstring simpleWork(const EmployeeState& e){
  const auto& a=CATALOG[e.abnormality];
  if(!a.regularWork)return L"不可工作";
  PlanInput p;p.level=a.level;p.boxes=expectedBoxes[p.level];p.hp=1.;p.sp=1.;
  static constexpr int caps[]={100,120,130};p.cap=caps[e.capIndex];
  p.tech=e.tech;p.resident=e.resident;p.clerks=e.clerks;
  p.current=e.current;p.target=e.target;p.completed=e.completed;
  auto section=L"prefs_"+std::wstring(a.id);
  for(int i=0;i<4;i++)for(int j=0;j<5;j++){
    p.rates[i][j]=a.rates[i][j];
    if(p.rates[i][j]==-101){
      auto key=L"r"+std::to_wstring(i)+L"_"+std::to_wstring(j);
      auto saved=readIni(section.c_str(),key.c_str(),L"");
      if(!saved.empty())p.rates[i][j]=std::clamp(_wtoi(saved.c_str()),-100,100);
    }
  }
  int bestWork=-1,bestRate=-1;bool allDone=true,missingRate=false;
  for(int i=0;i<4;i++){
    auto r=simulate(p,i);
    if(r.rate==-101){missingRate=true;allDone=false;}
    if(r.remaining>0||!r.reached)allDone=false;
    if(r.rate>0&&r.remaining>0&&r.rate>bestRate){bestRate=r.rate;bestWork=i;}
  }
  if(allDone)return L"已完成";
  if(bestWork>=0)return works[bestWork];
  return missingRate?L"待补录偏好":L"无推荐";
}
static void updateSimpleRow(int index){
  if(!simpleList||index<0||index>=(int)employees.size())return;
  const auto& e=employees[index];
  auto name=e.name.empty()?L"未命名员工":e.name;
  auto level=L"Lv."+std::to_wstring(agentLevel(e.current));
  auto object=std::wstring(CATALOG[e.abnormality].name);
  auto work=simpleWork(e);
  int total=0;for(int count:e.completed)total+=count;
  auto completed=std::to_wstring(total)+L" 次";
  ListView_SetItemText(simpleList,index,0,name.data());
  ListView_SetItemText(simpleList,index,1,level.data());
  ListView_SetItemText(simpleList,index,2,object.data());
  ListView_SetItemText(simpleList,index,3,work.data());
  ListView_SetItemText(simpleList,index,4,completed.data());
}
static void rebuildSimpleList(){
  if(!simpleList)return;
  hoveredSimpleRow=-1;
  ListView_DeleteAllItems(simpleList);
  for(int i=0;i<(int)employees.size();i++){
    LVITEMW item{};item.mask=LVIF_TEXT;item.iItem=i;
    item.pszText=employees[i].name.data();ListView_InsertItem(simpleList,&item);
    updateSimpleRow(i);
  }
}
static void updateNavigationRow(int index){
  if(!tabs||index<0||index>=(int)employees.size())return;
  const auto& e=employees[index];
  std::wstring name=e.name.empty()?L"未命名员工":e.name;
  TCITEMW tab{};tab.mask=TCIF_TEXT;tab.pszText=name.data();
  TabCtrl_SetItem(tabs,index,&tab);
  if(currentPage==1){
    InvalidateRect(win,nullptr,FALSE);
  }
}
static void rebuildNavigation(){
  bool wasReady=ready;ready=false;
  TabCtrl_DeleteAllItems(tabs);
  for(int i=0;i<(int)employees.size();i++){
    std::wstring name=employees[i].name;
    TCITEMW tab{};tab.mask=TCIF_TEXT;tab.pszText=name.data();
    TabCtrl_InsertItem(tabs,i,&tab);
    updateNavigationRow(i);
  }
  TabCtrl_SetCurSel(tabs,activeEmployee);
  if(activeEmployee<firstVisibleTab)firstVisibleTab=activeEmployee;
  if(activeEmployee>=firstVisibleTab+VISIBLE_TABS)
    firstVisibleTab=activeEmployee-VISIBLE_TABS+1;
  ready=wasReady;
}
static void saveState(){
  if(!ready)return;
  auto& e=employees[activeEmployee];
  e.name=getText(nameEdit);if(e.name.empty())e.name=L"未命名员工";
  e.abnormality=selected;
  e.capIndex=std::clamp((int)SendMessageW(capBox,CB_GETCURSEL,0,0),0,2);
  e.tech=SendMessageW(techBox,BM_GETCHECK,0,0)==BST_CHECKED;
  e.resident=std::clamp((int)SendMessageW(residentBox,CB_GETCURSEL,0,0),0,3);
  e.clerks=std::clamp((int)SendMessageW(clerksBox,CB_GETCURSEL,0,0),0,3);
  const int caps[]={100,120,130};double cap=caps[e.capIndex];
  for(int i=0;i<4;i++){
    e.current[i]=std::clamp(num(stats[i],e.current[i]),1.,cap);
    e.target[i]=std::clamp(num(targets[i],e.target[i]),e.current[i],cap);
    e.completed[i]=std::clamp(integer(done[i],e.completed[i]),0,10000);
  }
  writeEmployee(activeEmployee);
  if(CATALOG[selected].rates[0][0]==-101&&CATALOG[selected].regularWork){
    auto section=prefSection();
    for(int i=0;i<4;i++)for(int j=0;j<5;j++){
      auto key=L"r"+std::to_wstring(i)+L"_"+std::to_wstring(j);
      writeIni(section.c_str(),key.c_str(),getText(rates[i][j]));
    }
  }
  updateNavigationRow(activeEmployee);
  updateSimpleRow(activeEmployee);
}
static void fill(HDC dc,int x,int y,int w,int h,COLORREF color){
  RECT r{x,y,x+w,y+h};HBRUSH b=CreateSolidBrush(color);FillRect(dc,&r,b);DeleteObject(b);
}
static void rounded(HDC dc,int x,int y,int w,int h,COLORREF color,COLORREF edge=BORDER){
  HBRUSH b=CreateSolidBrush(color);HPEN p=CreatePen(PS_SOLID,1,edge);
  auto oldB=SelectObject(dc,b);auto oldP=SelectObject(dc,p);
  RoundRect(dc,x,y,x+w,y+h,18,18);
  SelectObject(dc,oldB);SelectObject(dc,oldP);DeleteObject(b);DeleteObject(p);
}
static void draw(HDC dc,const std::wstring& value,int x,int y,int w,int h,HFONT font,
                 COLORREF color,UINT align);
static void paintControlFrames(HDC dc){
  HPEN border=CreatePen(PS_SOLID,1,BORDER);
  HGDIOBJ oldPen=SelectObject(dc,border);
  HGDIOBJ oldBrush=SelectObject(dc,GetStockObject(NULL_BRUSH));
  for(const auto& frame:controlFrames)if(frame.page==currentPage){
    RECT area{};
    if(currentPage==1){
      int x,y,w,h;
      detailControlBounds(frame.item,frame.x,frame.y,frame.w,frame.h,x,y,w,h);
      area={x,y,x+w,y+h};
    }
    else{
      GetWindowRect(frame.item,&area);
      MapWindowPoints(HWND_DESKTOP,win,(POINT*)&area,2);
      area.left-=SIDEBAR_WIDTH;area.right-=SIDEBAR_WIDTH;
    }
    int diameter=area.bottom-area.top>60?16:12;
    RoundRect(dc,area.left,area.top,area.right,area.bottom,diameter,diameter);
  }
  SelectObject(dc,oldBrush);SelectObject(dc,oldPen);DeleteObject(border);
}
static void paintTabPills(HDC dc){
  const bool overflow=employees.size()>VISIBLE_TABS;
  if(overflow){
    rounded(dc,24,TAB_Y+2,27,TAB_HEIGHT-4,CARD,BORDER);
    rounded(dc,1040,TAB_Y+2,27,TAB_HEIGHT-4,CARD,BORDER);
    draw(dc,L"‹",25,TAB_Y+1,25,TAB_HEIGHT-2,bodyFont,
         firstVisibleTab?INK:MUTED,DT_CENTER);
    draw(dc,L"›",1041,TAB_Y+1,25,TAB_HEIGHT-2,bodyFont,
         firstVisibleTab+VISIBLE_TABS<(int)employees.size()?INK:MUTED,DT_CENTER);
  }
  for(int i=firstVisibleTab;i<(int)employees.size()&&i<firstVisibleTab+VISIBLE_TABS;i++){
    int x=TAB_X+(i-firstVisibleTab)*(TAB_WIDTH+TAB_GAP);
    const bool active=i==activeEmployee;
    rounded(dc,x,TAB_Y,TAB_WIDTH,TAB_HEIGHT,active?SOFT_RED:CARD,
            active?RGB(226,172,167):BORDER);
    draw(dc,employees[i].name,x+10,TAB_Y+1,TAB_WIDTH-20,TAB_HEIGHT-2,
         bodyFont,active?RED:INK,DT_CENTER);
  }
}
static void card(HDC dc,int x,int y,int w,int h){
  rounded(dc,x,y+3,w,h,SHADOW,SHADOW);
  rounded(dc,x,y,w,h,CARD);
}
static void line(HDC dc,int x1,int y1,int x2,int y2,COLORREF color){
  HPEN p=CreatePen(PS_SOLID,1,color);auto old=SelectObject(dc,p);
  MoveToEx(dc,x1,y1,nullptr);LineTo(dc,x2,y2);SelectObject(dc,old);DeleteObject(p);
}
static void draw(HDC dc,const std::wstring& value,int x,int y,int w,int h,HFONT font,
                 COLORREF color,UINT align=DT_LEFT){
  RECT r{x,y,x+w,y+h};auto old=SelectObject(dc,font);
  SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);
  DrawTextW(dc,value.c_str(),-1,&r,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX|align);
  SelectObject(dc,old);
}
static void section(HDC dc,const wchar_t* title,int x,int y,int width){
  fill(dc,x,y+5,4,16,RED);
  draw(dc,title,x+15,y,width-15,27,sectionFont,INK);
}
static void paintHeader(HDC dc,const wchar_t* title,int page){
  int width=1200+extraWidth();
  fill(dc,0,0,width,76,DARK);
  fill(dc,24,20,4,36,RED);
  draw(dc,title,45,9,std::min(745,width-255),43,titleFont,CARD);
  draw(dc,L"LOBOTOMY CORPORATION   /   TRAINING PLANNER",47,49,
       std::min(670,width-260),18,smallFont,RGB(177,190,188));
  rounded(dc,1010+extraWidth(),21,155,34,PANEL,PANEL);
  draw(dc,updateDownloading?L"●  正在下载更新":
          updateReady?L"●  更新待安装":
          page==1?L"●  自动保存":L"●  离线工作台",
       1018+extraWidth(),21,139,34,smallFont,RGB(190,224,205),DT_CENTER);
}
static void paintResults(HDC dc){
  COLORREF accent=trainingComplete?GREEN:RED;
  int dw=detailRightGrowth(),dy=extraHeight();
  rounded(dc,670,546+dy,486+dw,66,trainingComplete?SOFT_GREEN:SOFT_RED);
  fill(dc,670,558+dy,4,42,accent);
  if(best>=0){
    const wchar_t* prefix=L"推荐工作  ·  ";
    HGDIOBJ oldFont=SelectObject(dc,emphasisFont);
    SIZE size{};GetTextExtentPoint32W(dc,prefix,(int)wcslen(prefix),&size);
    SelectObject(dc,oldFont);
    draw(dc,prefix,688,550+dy,size.cx+2,31,emphasisFont,accent);
    draw(dc,works[best],688+size.cx,550+dy,450+dw-size.cx,31,emphasisFont,WORK_COLORS[best]);
  }else draw(dc,summary,688,550+dy,450+dw,31,emphasisFont,accent);
  draw(dc,detail,688,582+dy,450+dw,22,smallFont,INK);
  if(!CATALOG[selected].regularWork){
    draw(dc,L"请选择可进行常规工作的异想体。",674,650+dy,475+dw,32,bodyFont,MUTED);
    return;
  }
  draw(dc,L"工作",674,617+dy,65,22,smallFont,MUTED);
  draw(dc,L"属性",751+dw/4,617+dy,60,22,smallFont,MUTED);
  draw(dc,L"偏好",816+dw/2,617+dy,66,22,smallFont,MUTED);
  draw(dc,L"单次增益",892+dw*3/4,617+dy,88,22,smallFont,MUTED);
  draw(dc,L"计划 / 已做 / 剩余",992+dw,617+dy,163,22,smallFont,MUTED);
  for(int i=0;i<4;i++){
    int y=641+dy+i*22;auto r=viewResults[i];
    fill(dc,670,y,486+dw,21,i==best?WORK_BACKGROUNDS[i]:(i%2?CARD:ROW));
    if(i==best)fill(dc,670,y,3,21,WORK_COLORS[i]);
    draw(dc,works[i],677,y,60,21,bodyFont,WORK_COLORS[i]);
    draw(dc,virtues[i],751+dw/4,y,60,21,bodyFont,WORK_COLORS[i]);
    draw(dc,r.rate==-101?L"—":std::to_wstring(r.rate)+L"%",816+dw/2,y,70,21,bodyFont,
         r.rate>0?GREEN:MUTED);
    draw(dc,r.rate==-101?L"—":fmt(r.firstGain),892+dw*3/4,y,88,21,bodyFont,INK);
    std::wstring count=r.rate==-101?L"—":std::to_wstring(r.count)+L" / "+
                       std::to_wstring(viewPlan.completed[i])+L" / "+std::to_wstring(r.remaining);
    draw(dc,count,992+dw,y,163,21,bodyFont,INK);
  }
  if(viewPlan.level==3)draw(dc,L"WAW 连续工作过载未计入估算。",672,729+dy,482+dw,17,smallFont,MUTED);
}
static void paintQualityTable(HDC dc,int leftGrowth,int dy){
  const wchar_t* headers[]={L"品质等级",L"I",L"II",L"III",L"IV",L"V",L"EX"};
  const wchar_t* values[]={L"判定数值范围",L"1–29",L"30–44",L"45–64",L"65–84",L"85–100",L"100+"};
  const int x=45,top=713+dy,totalWidth=562+leftGrowth,rowHeight=25;
  const int labelWidth=217+leftGrowth/4;
  for(int row=0;row<2;row++){
    int y=top+row*rowHeight;
    fill(dc,x,y,totalWidth,rowHeight,row==0?HEADER_BG:(row%2?CARD:ROW));
    draw(dc,row==0?headers[0]:values[0],x+8,y,labelWidth-12,rowHeight,
         smallFont,row==0?INK:MUTED);
    for(int col=1;col<=6;col++){
      int left=x+labelWidth+(col-1)*(totalWidth-labelWidth)/6;
      int right=x+labelWidth+col*(totalWidth-labelWidth)/6;
      draw(dc,row==0?headers[col]:values[col],left,y,right-left,rowHeight,
           row==0?headerFont:smallFont,INK,DT_CENTER);
      line(dc,left,y+4,left,y+rowHeight-4,BORDER);
    }
    line(dc,x,y+rowHeight-1,x+totalWidth,y+rowHeight-1,BORDER);
  }
  draw(dc,L"员工等级共 I～V 五级，由四项品质等级总和决定。",
       x+8,765+dy,totalWidth-16,18,smallFont,MUTED);
  draw(dc,L"除 V 级外，等级为四项品质等级总和除以 3 向下取整；",
       x+8,783+dy,totalWidth-16,18,smallFont,MUTED);
  draw(dc,L"V 级要求品质等级总和 ≥ 16。",
       x+8,801+dy,totalWidth-16,18,smallFont,MUTED);
}
static void paintDetail(HDC dc){
  int leftGrowth=detailLeftGrowth(),rightGrowth=detailRightGrowth(),dy=extraHeight();
  RECT whole{0,0,1200+extraWidth(),DETAIL_LAYOUT_HEIGHT+dy};
  HBRUSH bg=CreateSolidBrush(BG);FillRect(dc,&whole,bg);DeleteObject(bg);
  paintHeader(dc,L"员工训练计算器",1);
  paintTabPills(dc);
  POINT previous{};GetViewportOrgEx(dc,&previous);
  SetViewportOrgEx(dc,previous.x,previous.y-DETAIL_SHIFT,nullptr);
  card(dc,22,268,608+leftGrowth,481+dy);
  section(dc,L"员工档案",45,282,540+leftGrowth);
  line(dc,46,404,605+leftGrowth,404,BORDER);
  section(dc,L"属性与目标",45,411,540+leftGrowth);
  line(dc,46,631+dy,605+leftGrowth,631+dy,BORDER);
  section(dc,L"训练条件",45,635+dy,540+leftGrowth);
  SetViewportOrgEx(dc,previous.x+leftGrowth,previous.y-DETAIL_SHIFT,nullptr);
  card(dc,648,268,530+rightGrowth,481+dy);
  section(dc,L"工作偏好",671,282,470);
  draw(dc,L"按员工品质 I–V 的基础成功率 %",671,310,470,22,smallFont,MUTED);
  for(int i=0;i<4;i++){
    int x=preferenceCellX(i,rightGrowth),w=preferenceCellWidth(rightGrowth);
    rounded(dc,x,337,w,25,WORK_BACKGROUNDS[i],WORK_BACKGROUNDS[i]);
    draw(dc,works[i],x,337,w,25,bodyFont,WORK_COLORS[i],DT_CENTER);
  }
  line(dc,672,514+dy,1154+rightGrowth,514+dy,BORDER);
  section(dc,L"训练结果",671,519+dy,470+rightGrowth);
  SetViewportOrgEx(dc,previous.x,previous.y,nullptr);
  card(dc,22,674+dy,608+leftGrowth,150);
  section(dc,L"员工品质与属性范围",45,685+dy,540+leftGrowth);
  paintQualityTable(dc,leftGrowth,dy);
  SetViewportOrgEx(dc,previous.x+leftGrowth,previous.y,nullptr);
  card(dc,648,674+dy,530+rightGrowth,150);
  section(dc,L"使用提示",671,685+dy,470+rightGrowth);
  draw(dc,L"员工切换：上方标签或简单模式双击员工",672,721+dy,465+rightGrowth,22,smallFont,MUTED);
  draw(dc,L"工作次数：使用 ＋ / − 或直接输入",672,751+dy,465+rightGrowth,22,smallFont,MUTED);
  draw(dc,L"所有修改自动保存到程序目录的 LobotomyCorpCal.ini",672,781+dy,
       465+rightGrowth,22,smallFont,MUTED);
  SetViewportOrgEx(dc,previous.x,previous.y,nullptr);
}
static void paintHelp(HDC dc){
  const int width=std::max(250,1090+extraWidth());
  const struct {const wchar_t* title;const wchar_t* explanation;} steps[]={
    {L"1  添加收容",L"在「当前收容」中添加已拥有的异想体。员工只能从这些对象中选择工作目标。"},
    {L"2  填写员工",L"在「详细模式」填写当前属性、目标属性和工作异想体，程序会估算各类工作次数。"},
    {L"3  记录进度",L"每完成一次工作，用「已完成」旁的 ＋ / − 调整次数；也可以直接输入。"},
    {L"4  查看资料",L"「简单模式」汇总员工；「收容推荐」和「E.G.O 库存」可供规划与记录。"}
  };
  for(int i=0;i<4;i++){
    int y=182+i*91;
    rounded(dc,46,y,width,76,i%2?CARD:ROW,BORDER);
    fill(dc,46,y+13,4,48,RED);
    draw(dc,steps[i].title,66,y+8,width-42,27,sectionFont,INK);
    draw(dc,steps[i].explanation,66,y+39,width-42,25,bodyFont,MUTED);
  }
  line(dc,46,570,46+width,570,BORDER);
  section(dc,L"保存与更新",46,587,width);
  draw(dc,L"所有输入自动保存至程序所在文件夹的 LobotomyCorpCal.ini；备份时请同时保留 exe 和 ini。",
       47,623,width-8,24,bodyFont,INK);
  draw(dc,L"启动时会检查新版本；离线也可正常使用。训练次数仅供规划，请以游戏内结果为准。",
       47,657,width-8,24,bodyFont,MUTED);
}
static void paintOtherPage(HDC dc){
  paintHeader(dc,pageNames[currentPage],currentPage);
  card(dc,22,92,1156+extraWidth(),656+extraHeight());
  const wchar_t* descriptions[]={
    L"全部员工的等级、工作对象、推荐工作类型和已完成总次数。双击员工可进入详细模式。",
    L"",
    L"在这里添加或移除当前收容的异想体。员工只能选择已收容对象。",
    L"参考工作簿的原始推荐评级。点击编号、等级或收容推荐的表头可切换排序。",
    L"按异想体或 E.G.O 名称搜索，支持汉字、拼音全拼和首字母；上限为 Wiki 列出的基础持有上限。",
    L"从添加收容到记录训练进度，按下面四步即可开始使用。"
  };
  section(dc,pageNames[currentPage],45,106,std::max(250,1090+extraWidth()));
  draw(dc,descriptions[currentPage],46,132,std::max(250,1080+extraWidth()),23,smallFont,MUTED);
  if(currentPage==2)
    draw(dc,containmentMessage,46,205,1080,23,smallFont,RED);
  if(currentPage==HELP_PAGE)paintHelp(dc);
}
static void paintSidebar(HDC dc){
  RECT client{};GetClientRect(win,&client);
  int height=client.bottom;
  fill(dc,0,0,SIDEBAR_WIDTH,height,DARK);
  fill(dc,0,0,SIDEBAR_WIDTH,4,RED);
  rounded(dc,20,23,44,42,RED,RED);
  draw(dc,L"LC",25,26,34,34,sectionFont,CARD,DT_CENTER);
  draw(dc,L"管理面板",20,75,155,27,sectionFont,CARD);
  line(dc,20,105,170,105,RGB(63,76,80));
  for(int i=0;i<PAGE_COUNT;i++){
    int y=112+i*64;
    int page=sidebarPages[i];
    if(page==currentPage){rounded(dc,8,y,174,52,PANEL,PANEL);fill(dc,8,y+9,4,34,RED);}
    draw(dc,pageNames[page],22,y+7,152,38,bodyFont,
         page==currentPage?CARD:RGB(191,198,197));
  }
  if(height>=520){
    line(dc,20,height-70,170,height-70,RGB(63,76,80));
    draw(dc,L"LOBOTOMY CORP",22,height-54,146,22,smallFont,RGB(144,160,160));
  }
}
static void paint(HDC dc){
  RECT whole{};GetClientRect(win,&whole);HBRUSH bg=CreateSolidBrush(BG);
  FillRect(dc,&whole,bg);DeleteObject(bg);
  POINT previous{};SetViewportOrgEx(dc,SIDEBAR_WIDTH-scrollX,-scrollY,&previous);
  if(currentPage==1){
    paintDetail(dc);
    if(ready){
      POINT detailOrigin{};GetViewportOrgEx(dc,&detailOrigin);
      SetViewportOrgEx(dc,detailOrigin.x+detailLeftGrowth(),
                         detailOrigin.y-DETAIL_SHIFT,nullptr);
      paintResults(dc);
      SetViewportOrgEx(dc,detailOrigin.x,detailOrigin.y,nullptr);
    }
  }
  else paintOtherPage(dc);
  paintControlFrames(dc);
  SetViewportOrgEx(dc,previous.x,previous.y,nullptr);
  paintSidebar(dc);
}
static void paintButton(const DRAWITEMSTRUCT* item){
  RECT area=item->rcItem;
  FillRect(item->hDC,&area,cardBrush);
  const bool disabled=(item->itemState&ODS_DISABLED)!=0;
  const bool pressed=(item->itemState&ODS_SELECTED)!=0;
  const int id=(int)item->CtlID;
  const bool primary=id==500||id==700;
  const bool destructive=id==701;
  const bool increment=(id>=400&&id<=407&&id%2==1)||
                       (id>=710&&id<=713&&id%2==1);
  const bool decrement=(id>=400&&id<=407&&id%2==0)||
                       (id>=710&&id<=713&&id%2==0);
  COLORREF background=CARD,edge=BORDER,foreground=INK;
  if(primary){background=RED;edge=RED;foreground=CARD;}
  else if(increment){background=SOFT_GREEN;edge=RGB(196,226,208);foreground=GREEN;}
  else if(destructive||decrement){background=SOFT_RED;edge=RGB(238,207,203);foreground=RED;}
  if(disabled){background=RGB(244,246,245);edge=BORDER;foreground=RGB(160,170,170);}
  else if(pressed){background=primary?RGB(142,51,47):RGB(229,235,232);}
  rounded(item->hDC,area.left+1,area.top+1,area.right-area.left-2,
          area.bottom-area.top-2,background,edge);
  wchar_t caption[64]{};
  GetWindowTextW(item->hwndItem,caption,64);
  if(pressed)OffsetRect(&area,0,1);
  draw(item->hDC,caption,area.left+4,area.top,area.right-area.left-8,
       area.bottom-area.top,currentBodyFont(),foreground,DT_CENTER);
  if(item->itemState&ODS_FOCUS){
    RECT focus=item->rcItem;InflateRect(&focus,-4,-4);
    DrawFocusRect(item->hDC,&focus);
  }
}
static void paintCombo(const DRAWITEMSTRUCT* item){
  RECT area=item->rcItem;
  const bool inBox=(item->itemState&ODS_COMBOBOXEDIT)!=0;
  if(inBox)return;
  const bool selected=(item->itemState&ODS_SELECTED)!=0&&!inBox;
  const bool disabled=(item->itemState&ODS_DISABLED)!=0;
  HBRUSH background=CreateSolidBrush(selected?SOFT_RED:FIELD);
  FillRect(item->hDC,&area,background);DeleteObject(background);
  if(item->itemID==(UINT)-1)return;
  wchar_t caption[256]{};
  SendMessageW(item->hwndItem,CB_GETLBTEXT,item->itemID,(LPARAM)caption);
  draw(item->hDC,caption,area.left+9,area.top,area.right-area.left-16,
       area.bottom-area.top,currentBodyFont(),disabled?MUTED:(selected?RED:INK));
}
static LRESULT paintTableHeader(const NMHDR* notice){
  auto* custom=(NMCUSTOMDRAW*)notice;
  if(custom->dwDrawStage==CDDS_PREPAINT){
    RECT area{};GetClientRect(notice->hwndFrom,&area);
    HBRUSH background=CreateSolidBrush(HEADER_BG);
    FillRect(custom->hdc,&area,background);DeleteObject(background);
    return CDRF_NOTIFYITEMDRAW;
  }
  if(custom->dwDrawStage==CDDS_ITEMPREPAINT){
    RECT area=custom->rc;
    HBRUSH background=CreateSolidBrush(HEADER_BG);
    FillRect(custom->hdc,&area,background);DeleteObject(background);
    wchar_t caption[256]{};
    HDITEMW column{};column.mask=HDI_TEXT|HDI_FORMAT;
    column.pszText=caption;column.cchTextMax=256;
    Header_GetItem(notice->hwndFrom,(int)custom->dwItemSpec,&column);
    const bool sorted=(column.fmt&(HDF_SORTUP|HDF_SORTDOWN))!=0;
    draw(custom->hdc,caption,area.left+11,area.top,area.right-area.left-(sorted?30:18),
         area.bottom-area.top,currentHeaderFont(),INK);
    if(sorted)draw(custom->hdc,(column.fmt&HDF_SORTUP)?L"↑":L"↓",
                   area.right-25,area.top,15,area.bottom-area.top,currentHeaderFont(),RED,DT_CENTER);
    line(custom->hdc,area.left,area.bottom-1,area.right,area.bottom-1,
         sorted?RED:BORDER);
    line(custom->hdc,area.right-1,area.top+7,area.right-1,area.bottom-7,BORDER);
    return CDRF_SKIPDEFAULT;
  }
  return CDRF_DODEFAULT;
}
static LRESULT paintContainedRow(const NMLVCUSTOMDRAW* custom){
  int row=(int)custom->nmcd.dwItemSpec;
  LVITEMW item{};item.mask=LVIF_PARAM;item.iItem=row;
  if(!ListView_GetItem(containedList,&item)||item.lParam<0||item.lParam>=CATALOG_SIZE)
    return CDRF_DODEFAULT;
  int level=CATALOG[item.lParam].level;
  RECT rowRect{},client{},nameRect{},levelRect{};
  if(!ListView_GetItemRect(containedList,row,&rowRect,LVIR_BOUNDS)||
     !ListView_GetSubItemRect(containedList,row,0,LVIR_LABEL,&nameRect)||
     !ListView_GetSubItemRect(containedList,row,1,LVIR_BOUNDS,&levelRect))
    return CDRF_DODEFAULT;
  GetClientRect(containedList,&client);
  COLORREF accent=RISK_COLORS[level];
  COLORREF foreground=level==2?RGB(128,105,0):accent;
  bool selected=(ListView_GetItemState(containedList,row,LVIS_SELECTED)&LVIS_SELECTED)!=0;
  bool hovered=row==hoveredContainedRow;
  int tint=selected?30:(hovered?22:14);
  auto lighten=[tint](BYTE component){return (component*tint+255*(100-tint))/100;};
  COLORREF background=RGB(lighten(GetRValue(accent)),lighten(GetGValue(accent)),
                          lighten(GetBValue(accent)));
  int height=rowRect.bottom-rowRect.top;
  fill(custom->nmcd.hdc,0,rowRect.top,client.right,height,CARD);
  int pillLeft=std::max(6,(int)rowRect.left+6);
  int pillRight=std::min((int)client.right-6,
                         std::max((int)rowRect.right,(int)levelRect.right)-6);
  rounded(custom->nmcd.hdc,pillLeft,rowRect.top+2,std::max(0,pillRight-pillLeft),
          std::max(0,height-4),background,background);
  if(selected)fill(custom->nmcd.hdc,pillLeft+3,rowRect.top+5,3,
                   std::max(0,height-10),foreground);
  std::wstring name=std::wstring(CATALOG[item.lParam].id)+L"  "+CATALOG[item.lParam].name;
  draw(custom->nmcd.hdc,name,nameRect.left+14,rowRect.top+2,
       std::max(0,(int)(levelRect.left-nameRect.left-20)),height-4,
       bodyFont,foreground);
  draw(custom->nmcd.hdc,levels[level],levelRect.left+10,rowRect.top+2,
       std::max(0,(int)(levelRect.right-levelRect.left-18)),height-4,
       bodyFont,foreground);
  return CDRF_SKIPDEFAULT;
}
static LRESULT paintSimpleRow(const NMLVCUSTOMDRAW* custom){
  int row=(int)custom->nmcd.dwItemSpec;
  if(row<0||row>=(int)employees.size())return CDRF_DODEFAULT;
  int level=std::clamp(agentLevel(employees[row].current),1,5)-1;
  RECT rowRect{},client{},cells[5]{};
  if(!ListView_GetItemRect(simpleList,row,&rowRect,LVIR_BOUNDS)||
     !ListView_GetSubItemRect(simpleList,row,0,LVIR_LABEL,&cells[0]))
    return CDRF_DODEFAULT;
  for(int column=1;column<5;column++)
    if(!ListView_GetSubItemRect(simpleList,row,column,LVIR_BOUNDS,&cells[column]))
      return CDRF_DODEFAULT;
  GetClientRect(simpleList,&client);
  COLORREF accent=RISK_COLORS[level];
  COLORREF foreground=level==2?RGB(128,105,0):accent;
  bool selected=(ListView_GetItemState(simpleList,row,LVIS_SELECTED)&LVIS_SELECTED)!=0;
  int tint=selected?30:(row==hoveredSimpleRow?22:14);
  auto lighten=[tint](BYTE component){return (component*tint+255*(100-tint))/100;};
  COLORREF background=RGB(lighten(GetRValue(accent)),lighten(GetGValue(accent)),
                          lighten(GetBValue(accent)));
  int height=rowRect.bottom-rowRect.top;
  fill(custom->nmcd.hdc,0,rowRect.top,client.right,height,CARD);
  int pillLeft=std::max(6,(int)rowRect.left+6);
  int pillRight=std::min((int)client.right-6,
                         std::max((int)rowRect.right,(int)cells[4].right)-6);
  rounded(custom->nmcd.hdc,pillLeft,rowRect.top+2,std::max(0,pillRight-pillLeft),
          std::max(0,height-4),background,background);
  if(selected)fill(custom->nmcd.hdc,pillLeft+3,rowRect.top+5,3,
                   std::max(0,height-10),foreground);
  for(int column=0;column<5;column++){
    wchar_t value[256]{};
    ListView_GetItemText(simpleList,row,column,value,256);
    int left=cells[column].left+12;
    int right=cells[column].right-10;
    draw(custom->nmcd.hdc,value,left,rowRect.top+2,std::max(0,right-left),height-4,
         bodyFont,foreground);
  }
  return CDRF_SKIPDEFAULT;
}
static LRESULT paintEgoRow(const NMLVCUSTOMDRAW* custom){
  int row=(int)custom->nmcd.dwItemSpec;
  if(row<0||row>=(int)egoOrder.size())return CDRF_DODEFAULT;
  int level=CATALOG[egoOrder[row]].level;
  RECT rowRect{},client{},cells[9]{};
  if(!ListView_GetItemRect(egoList,row,&rowRect,LVIR_BOUNDS)||
     !ListView_GetSubItemRect(egoList,row,0,LVIR_LABEL,&cells[0]))
    return CDRF_DODEFAULT;
  for(int column=1;column<9;column++)
    if(!ListView_GetSubItemRect(egoList,row,column,LVIR_BOUNDS,&cells[column]))
      return CDRF_DODEFAULT;
  GetClientRect(egoList,&client);
  COLORREF accent=RISK_COLORS[level];
  COLORREF foreground=level==2?RGB(128,105,0):accent;
  bool selected=(ListView_GetItemState(egoList,row,LVIS_SELECTED)&LVIS_SELECTED)!=0;
  int tint=selected?30:(row==hoveredEgoRow?22:14);
  auto lighten=[tint](BYTE component){return (component*tint+255*(100-tint))/100;};
  COLORREF background=RGB(lighten(GetRValue(accent)),lighten(GetGValue(accent)),
                          lighten(GetBValue(accent)));
  int height=rowRect.bottom-rowRect.top;
  fill(custom->nmcd.hdc,0,rowRect.top,client.right,height,CARD);
  int pillLeft=std::max(6,(int)rowRect.left+6);
  int pillRight=std::min((int)client.right-6,
                         std::max((int)rowRect.right,(int)cells[8].right)-6);
  rounded(custom->nmcd.hdc,pillLeft,rowRect.top+2,std::max(0,pillRight-pillLeft),
          std::max(0,height-4),background,background);
  if(selected)fill(custom->nmcd.hdc,pillLeft+3,rowRect.top+5,3,
                   std::max(0,height-10),foreground);
  for(int column=0;column<9;column++){
    wchar_t value[256]{};
    ListView_GetItemText(egoList,row,column,value,256);
    int left=cells[column].left+10;
    int right=cells[column].right-8;
    draw(custom->nmcd.hdc,value,left,rowRect.top+2,std::max(0,right-left),height-4,
         bodyFont,foreground);
  }
  return CDRF_SKIPDEFAULT;
}
static LRESULT paintRecommendationRow(const NMLVCUSTOMDRAW* custom){
  int row=(int)custom->nmcd.dwItemSpec;
  if(row<0||row>=(int)recommendationOrder.size())return CDRF_DODEFAULT;
  int level=CATALOG[recommendationOrder[row]].level;
  RECT rowRect{},client{},cells[7]{};
  if(!ListView_GetItemRect(recommendationList,row,&rowRect,LVIR_BOUNDS)||
     !ListView_GetSubItemRect(recommendationList,row,0,LVIR_LABEL,&cells[0]))
    return CDRF_DODEFAULT;
  for(int column=1;column<7;column++)
    if(!ListView_GetSubItemRect(recommendationList,row,column,LVIR_BOUNDS,&cells[column]))
      return CDRF_DODEFAULT;
  GetClientRect(recommendationList,&client);
  COLORREF accent=RISK_COLORS[level];
  COLORREF foreground=level==2?RGB(128,105,0):accent;
  bool selected=(ListView_GetItemState(recommendationList,row,LVIS_SELECTED)&LVIS_SELECTED)!=0;
  int tint=selected?30:(row==hoveredRecommendationRow?22:14);
  auto lighten=[tint](BYTE component){return (component*tint+255*(100-tint))/100;};
  COLORREF background=RGB(lighten(GetRValue(accent)),lighten(GetGValue(accent)),
                          lighten(GetBValue(accent)));
  int height=rowRect.bottom-rowRect.top;
  fill(custom->nmcd.hdc,0,rowRect.top,client.right,height,CARD);
  int pillLeft=std::max(6,(int)rowRect.left+6);
  int pillRight=std::min((int)client.right-6,
                         std::max((int)rowRect.right,(int)cells[6].right)-6);
  rounded(custom->nmcd.hdc,pillLeft,rowRect.top+2,std::max(0,pillRight-pillLeft),
          std::max(0,height-4),background,background);
  if(selected)fill(custom->nmcd.hdc,pillLeft+3,rowRect.top+5,3,
                   std::max(0,height-10),foreground);
  for(int column=0;column<7;column++){
    wchar_t value[512]{};
    ListView_GetItemText(recommendationList,row,column,value,512);
    int left=cells[column].left+10;
    int right=cells[column].right-8;
    draw(custom->nmcd.hdc,value,left,rowRect.top+2,std::max(0,right-left),height-4,
         bodyFont,foreground);
  }
  return CDRF_SKIPDEFAULT;
}
static LRESULT CALLBACK coloredListSubclass(HWND list,UINT message,WPARAM wp,LPARAM lp,
                                            UINT_PTR,DWORD_PTR){
  int& hovered=list==simpleList?hoveredSimpleRow:
               list==containedList?hoveredContainedRow:
               list==recommendationList?hoveredRecommendationRow:hoveredEgoRow;
  bool& tracking=list==simpleList?trackingSimpleMouse:
                 list==containedList?trackingContainedMouse:
                 list==recommendationList?trackingRecommendationMouse:trackingEgoMouse;
  if(message==WM_MOUSEMOVE){
    if(!tracking){
      TRACKMOUSEEVENT event{sizeof(TRACKMOUSEEVENT),TME_LEAVE,list,0};
      tracking=TrackMouseEvent(&event)!=FALSE;
    }
    LVHITTESTINFO hit{};
    hit.pt={(short)LOWORD(lp),(short)HIWORD(lp)};
    int row=ListView_HitTest(list,&hit);
    if(row!=hovered){
      int previous=hovered;
      hovered=row;
      if(previous>=0)ListView_RedrawItems(list,previous,previous);
      if(row>=0)ListView_RedrawItems(list,row,row);
    }
  }else if(message==WM_MOUSELEAVE){
    tracking=false;
    int previous=hovered;
    hovered=-1;
    if(previous>=0)ListView_RedrawItems(list,previous,previous);
  }else if(message==WM_NCDESTROY){
    RemoveWindowSubclass(list,coloredListSubclass,1);
  }
  return DefSubclassProc(list,message,wp,lp);
}
static void addColumn(HWND list,int index,const wchar_t* heading,int width){
  LVCOLUMNW column{};column.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM;
  column.pszText=const_cast<wchar_t*>(heading);column.cx=width;column.iSubItem=index;
  ListView_InsertColumn(list,index,&column);
  HWND header=ListView_GetHeader(list);
  SendMessageW(header,WM_SETFONT,(WPARAM)headerFont,TRUE);
  roundControl(header,12);
}
static void rebuildWorkChoices(){
  SendMessageW(abnoBox,CB_RESETCONTENT,0,0);
  int position=-1;
  for(int i=0;i<CATALOG_SIZE;i++)if(contained[i]){
    std::wstring name=std::wstring(CATALOG[i].id)+L"  "+CATALOG[i].name;
    int row=(int)SendMessageW(abnoBox,CB_ADDSTRING,0,(LPARAM)name.c_str());
    SendMessageW(abnoBox,CB_SETITEMDATA,row,i);
    if(i==selected)position=row;
  }
  if(position<0&&SendMessageW(abnoBox,CB_GETCOUNT,0,0)>0){
    position=0;selected=(int)SendMessageW(abnoBox,CB_GETITEMDATA,0,0);
  }
  SendMessageW(abnoBox,CB_SETCURSEL,position,0);
}
static void rebuildContainedLists(){
  if(!containedList||!availableCombo)return;
  hoveredContainedRow=-1;
  ListView_DeleteAllItems(containedList);
  SendMessageW(availableCombo,CB_RESETCONTENT,0,0);
  int row=0;
  for(int i=0;i<CATALOG_SIZE;i++){
    std::wstring name=std::wstring(CATALOG[i].id)+L"  "+CATALOG[i].name;
    if(contained[i]){
      LVITEMW item{};item.mask=LVIF_TEXT|LVIF_PARAM;item.iItem=row++;
      item.pszText=name.data();item.lParam=i;ListView_InsertItem(containedList,&item);
      auto level=std::wstring(levels[CATALOG[i].level]);
      ListView_SetItemText(containedList,item.iItem,1,level.data());
    }else{
      int position=(int)SendMessageW(availableCombo,CB_ADDSTRING,0,(LPARAM)name.c_str());
      SendMessageW(availableCombo,CB_SETITEMDATA,position,i);
    }
  }
  if(SendMessageW(availableCombo,CB_GETCOUNT,0,0)>0)
    SendMessageW(availableCombo,CB_SETCURSEL,0,0);
}
static std::wstring oneLine(const wchar_t* value){
  std::wstring result=value;
  for(auto& ch:result)if(ch==L'\n'||ch==L'\r')ch=L' ';
  return result.empty()?L"—":result;
}
static std::wstring egoRecommendationText(const Abnormality& abnormality,bool weapon,bool tooltip){
  if(weapon?abnormality.egoWeaponMax==0:abnormality.egoSuitMax==0)return L"—";
  const wchar_t* source=weapon?abnormality.weaponRecommendation:
                               abnormality.suitRecommendation;
  if(wcscmp(abnormality.id,L"T-03-46")==0)
    return tooltip?std::wstring(source):oneLine(source);
  return summarizeEgoRecommendation(source);
}
static void showRecommendationDetail(int index){
  if(index<0||index>=CATALOG_SIZE)return;
  const auto& a=CATALOG[index];
  std::wstring text=std::wstring(a.id)+L"  "+a.name+L"  /  "+levels[a.level]+
    L"\r\n收容推荐："+(std::wstring(a.containment).empty()?L"未填写":a.containment)+
    L"\r\nE.G.O 武器推荐："+(std::wstring(a.weaponRecommendation).empty()?L"未填写":a.weaponRecommendation)+
    L"\r\nE.G.O 防具推荐："+(std::wstring(a.suitRecommendation).empty()?L"未填写":a.suitRecommendation)+
    L"\r\n镇压："+(std::wstring(a.suppression).empty()?L"原表未填写":a.suppression);
  setText(recommendationDetail,text);
}
static int recommendationRank(const wchar_t* value){
  const std::wstring text=value;
  if(text==L"论外")return 1;
  if(text==L"高")return 2;
  if(text==L"中")return 3;
  if(text==L"低")return 4;
  if(text==L"极低")return 5;
  return 0; // 含附加说明或其他原表文字
}
static int lastIdNumber(const wchar_t* id){
  const wchar_t* last=wcsrchr(id,L'-');
  return last?_wtoi(last+1):0;
}
static void markRecommendationSort(){
  HWND header=ListView_GetHeader(recommendationList);
  for(int column:{0,2,3}){
    HDITEMW item{};item.mask=HDI_FORMAT;
    if(Header_GetItem(header,column,&item)){
      item.fmt&=~(HDF_SORTUP|HDF_SORTDOWN);
      if(column==recommendationSortColumn)
        item.fmt|=recommendationDescending?HDF_SORTDOWN:HDF_SORTUP;
      Header_SetItem(header,column,&item);
    }
  }
}
static void rebuildRecommendationList(){
  int selectedCatalog=-1;
  int selectedRow=ListView_GetNextItem(recommendationList,-1,LVNI_SELECTED);
  if(selectedRow>=0&&selectedRow<(int)recommendationOrder.size())
    selectedCatalog=recommendationOrder[selectedRow];
  hoveredRecommendationRow=-1;
  recommendationOrder.clear();
  for(int i=0;i<CATALOG_SIZE;i++)recommendationOrder.push_back(i);
  std::stable_sort(recommendationOrder.begin(),recommendationOrder.end(),[](int left,int right){
    int a=0,b=0;
    if(recommendationSortColumn==0){a=lastIdNumber(CATALOG[left].id);b=lastIdNumber(CATALOG[right].id);}
    else if(recommendationSortColumn==2){a=CATALOG[left].level;b=CATALOG[right].level;}
    else {a=recommendationRank(CATALOG[left].containment);b=recommendationRank(CATALOG[right].containment);}
    if(a==b)return left<right;
    return recommendationDescending?a>b:a<b;
  });
  bool wasReady=ready;ready=false;
  ListView_DeleteAllItems(recommendationList);
  int rowToSelect=0;
  for(int row=0;row<CATALOG_SIZE;row++){
    int i=recommendationOrder[row];
    if(i==selectedCatalog)rowToSelect=row;
    const auto& a=CATALOG[i];
    LVITEMW item{};item.mask=LVIF_TEXT|LVIF_PARAM;item.iItem=row;item.lParam=i;
    item.pszText=const_cast<wchar_t*>(a.id);ListView_InsertItem(recommendationList,&item);
    auto name=std::wstring(a.name),level=std::wstring(levels[a.level]);
    auto containment=oneLine(a.containment),weapon=oneLine(a.weaponRecommendation);
    auto suit=oneLine(a.suitRecommendation),suppression=oneLine(a.suppression);
    ListView_SetItemText(recommendationList,row,1,name.data());
    ListView_SetItemText(recommendationList,row,2,level.data());
    ListView_SetItemText(recommendationList,row,3,containment.data());
    ListView_SetItemText(recommendationList,row,4,weapon.data());
    ListView_SetItemText(recommendationList,row,5,suit.data());
    ListView_SetItemText(recommendationList,row,6,suppression.data());
  }
  ListView_SetItemState(recommendationList,rowToSelect,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
  ListView_EnsureVisible(recommendationList,rowToSelect,FALSE);
  showRecommendationDetail(recommendationOrder[rowToSelect]);
  markRecommendationSort();
  ready=wasReady;
}
static void updateEgoRow(int index){
  auto found=std::find(egoOrder.begin(),egoOrder.end(),index);
  if(found==egoOrder.end())return;
  int row=(int)std::distance(egoOrder.begin(),found);
  const auto& a=CATALOG[index];
  auto weapon=a.egoWeaponMax?std::to_wstring(egoWeapons[index])+L" / "+std::to_wstring(a.egoWeaponMax):L"—";
  auto suit=a.egoSuitMax?std::to_wstring(egoSuits[index])+L" / "+std::to_wstring(a.egoSuitMax):L"—";
  const wchar_t* status=egoWeapons[index]||egoSuits[index]?L"已解锁":L"未解锁";
  auto egoName=std::wstring(a.egoWeaponMax?a.egoWeaponName:a.egoSuitName);
  auto weaponRecommendation=egoRecommendationText(a,true,false);
  auto suitRecommendation=egoRecommendationText(a,false,false);
  ListView_SetItemText(egoList,row,2,const_cast<wchar_t*>(levels[a.level]));
  ListView_SetItemText(egoList,row,3,egoName.data());
  ListView_SetItemText(egoList,row,4,weapon.data());
  ListView_SetItemText(egoList,row,5,weaponRecommendation.data());
  ListView_SetItemText(egoList,row,6,suit.data());
  ListView_SetItemText(egoList,row,7,suitRecommendation.data());
  ListView_SetItemText(egoList,row,8,const_cast<wchar_t*>(status));
}
static int selectedEgoIndex(){
  int row=ListView_GetNextItem(egoList,-1,LVNI_SELECTED);
  return row>=0&&row<(int)egoOrder.size()?egoOrder[row]:-1;
}
static void updateEgoButtons(){
  int index=selectedEgoIndex();
  bool valid=index>=0;
  EnableWindow(egoButtons[0],valid&&egoWeapons[index]>0);
  EnableWindow(egoButtons[1],valid&&egoWeapons[index]<CATALOG[index].egoWeaponMax);
  EnableWindow(egoButtons[2],valid&&egoSuits[index]>0);
  EnableWindow(egoButtons[3],valid&&egoSuits[index]<CATALOG[index].egoSuitMax);
}
static void markEgoSort(){
  HWND header=ListView_GetHeader(egoList);
  for(int column:{0,2}){
    HDITEMW item{};item.mask=HDI_FORMAT;
    if(!Header_GetItem(header,column,&item))continue;
    item.fmt&=~(HDF_SORTUP|HDF_SORTDOWN);
    if(column==egoSortColumn)item.fmt|=egoSortDescending?HDF_SORTDOWN:HDF_SORTUP;
    Header_SetItem(header,column,&item);
  }
}
static void rebuildEgoList(){
  int previouslySelected=selectedEgoIndex();
  bool wasReady=ready;ready=false;
  hoveredEgoRow=-1;
  ListView_DeleteAllItems(egoList);
  egoOrder.clear();
  const auto query=getText(egoSearch);
  int availableCount=0;
  for(int i=0;i<CATALOG_SIZE;i++){
    const auto& a=CATALOG[i];
    if(a.egoWeaponMax==0&&a.egoSuitMax==0)continue;
    availableCount++;
    if(!matchesEgoSearch(a,query))continue;
    egoOrder.push_back(i);
  }
  std::stable_sort(egoOrder.begin(),egoOrder.end(),[](int left,int right){
    bool leftUnlocked=egoWeapons[left]>0||egoSuits[left]>0;
    bool rightUnlocked=egoWeapons[right]>0||egoSuits[right]>0;
    if(leftUnlocked!=rightUnlocked)return leftUnlocked;
    if(egoSortColumn==2&&CATALOG[left].level!=CATALOG[right].level)
      return egoSortDescending?CATALOG[left].level>CATALOG[right].level:
                               CATALOG[left].level<CATALOG[right].level;
    int leftNumber=lastIdNumber(CATALOG[left].id);
    int rightNumber=lastIdNumber(CATALOG[right].id);
    if(leftNumber!=rightNumber)
      return egoSortColumn==0&&egoSortDescending?leftNumber>rightNumber:
                                                  leftNumber<rightNumber;
    return false;
  });
  for(int row=0;row<(int)egoOrder.size();row++){
    int i=egoOrder[row];const auto& a=CATALOG[i];
    LVITEMW item{};item.mask=LVIF_TEXT|LVIF_PARAM;item.iItem=row;item.lParam=i;
    item.pszText=const_cast<wchar_t*>(a.id);ListView_InsertItem(egoList,&item);
    auto name=std::wstring(a.name);ListView_SetItemText(egoList,row,1,name.data());
    updateEgoRow(i);
  }
  if(!egoOrder.empty()){
    auto found=std::find(egoOrder.begin(),egoOrder.end(),previouslySelected);
    int row=found==egoOrder.end()?0:(int)std::distance(egoOrder.begin(),found);
    ListView_SetItemState(egoList,row,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
    ListView_EnsureVisible(egoList,row,FALSE);
  }
  setText(egoSearchCount,L"显示 "+std::to_wstring(egoOrder.size())+L" / "+std::to_wstring(availableCount));
  markEgoSort();
  ready=wasReady;
  updateEgoButtons();
}
static void layoutControls();
static void switchPage(int page){
  if(page<0||page>=PAGE_COUNT||page==currentPage)return;
  saveState();currentPage=page;
  for(int i=0;i<PAGE_COUNT;i++)for(HWND item:pageControls[i])ShowWindow(item,i==page?SW_SHOW:SW_HIDE);
  if(page==0)rebuildSimpleList();
  layoutControls();
  InvalidateRect(win,nullptr,FALSE);
}
static void loadActiveEmployee(){
  ready=false;
  const auto& e=employees[activeEmployee];selected=e.abnormality;
  setText(nameEdit,e.name);
  rebuildWorkChoices();
  setText(riskText,levels[CATALOG[selected].level]);
  SendMessageW(capBox,CB_SETCURSEL,e.capIndex,0);
  for(int i=0;i<4;i++){
    setText(stats[i],fmt(e.current[i],2));
    setText(targets[i],fmt(e.target[i],2));
    setText(done[i],std::to_wstring(e.completed[i]));
  }
  SendMessageW(techBox,BM_SETCHECK,e.tech?BST_CHECKED:BST_UNCHECKED,0);
  SendMessageW(residentBox,CB_SETCURSEL,e.resident,0);
  SendMessageW(clerksBox,CB_SETCURSEL,e.clerks,0);
  fillRates();
  rebuildNavigation();
  updateOutput();
}
static void switchEmployee(int index){
  if(index<0||index>=(int)employees.size()||index==activeEmployee)return;
  saveState();activeEmployee=index;
  if(activeEmployee<firstVisibleTab)firstVisibleTab=activeEmployee;
  if(activeEmployee>=firstVisibleTab+VISIBLE_TABS)
    firstVisibleTab=activeEmployee-VISIBLE_TABS+1;
  loadActiveEmployee();
  writeIni(L"employees",L"selected",std::to_wstring(activeEmployee));
  InvalidateRect(win,nullptr,FALSE);
}
static void initialize(){
  ready=false;
  bodyFont=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
  smallFont=CreateFontW(-13,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
  titleFont=CreateFontW(-31,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
  sectionFont=CreateFontW(-19,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
  emphasisFont=CreateFontW(-22,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
  headerFont=CreateFontW(-15,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
  cardBrush=CreateSolidBrush(CARD);editBrush=CreateSolidBrush(FIELD);
  tabs=CreateWindowExW(0,WC_TABCONTROLW,L"",WS_CHILD|TCS_SINGLELINE,
                       0,0,1,1,win,(HMENU)(INT_PTR)601,GetModuleHandleW(nullptr),nullptr);
  label(L"员工姓名",47,312,195);nameEdit=edit(47,338,221,101);
  abnoLabel=label(L"工作异想体",286,312,322);
  abnoBox=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,286,338,319,300,102);
  label(L"危险等级",47,375,100);
  riskText=control(L"STATIC",L"",SS_CENTER,153,374,115,28);
  SendMessageW(riskText,WM_SETFONT,(WPARAM)sectionFont,TRUE);
  capLabel=label(L"属性上限",286,375,100);
  capBox=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST,389,373,216,120,146);
  for(const wchar_t* value:{L"100  常规",L"120  记录部研究",L"130  核心抑制"})
    SendMessageW(capBox,CB_ADDSTRING,0,(LPARAM)value);
  attributeHeaders[0]=label(L"属性",48,444,90);
  attributeHeaders[1]=label(L"当前",160,444,90);
  attributeHeaders[2]=label(L"目标",274,444,90);
  attributeHeaders[3]=label(L"已完成",402,444,110);
  for(int i=0;i<4;i++){
    int y=470+i*38;
    virtueLabels[i]=label(virtues[i],48,y+4,88,25);
    stats[i]=edit(160,y,78,110+i);
    targets[i]=edit(274,y,78,120+i);
    done[i]=edit(402,y,78,130+i);
    doneMinus[i]=control(L"BUTTON",L"−",BS_PUSHBUTTON,487,y,30,29,400+i*2);
    donePlus[i]=control(L"BUTTON",L"+",BS_PUSHBUTTON,523,y,30,29,401+i*2);
  }
  techBox=control(L"BUTTON",L"普及培训手册  +0.5",BS_AUTOCHECKBOX,47,673,235,27,140);
  residentLabel=label(L"培训部常驻",303,652,135);
  residentBox=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST,303,676,125,110,141);
  clerksLabel=label(L"文职加成",447,652,130);
  clerksBox=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST,447,676,125,110,142);
  for(const wchar_t* value:{L"无",L"1级",L"2级",L"3级"}){
    SendMessageW(residentBox,CB_ADDSTRING,0,(LPARAM)value);
    SendMessageW(clerksBox,CB_ADDSTRING,0,(LPARAM)value);
  }
  for(int j=0;j<5;j++){
    std::wstring text=L"品质 "+std::to_wstring(j+1);
    label(text.c_str(),672,369+j*29,66,26);
    for(int i=0;i<4;i++)
      rates[i][j]=control(L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|ES_CENTER,
                          745+i*100,369+j*29,82,26,200+i*5+j);
  }
  pageControl(0,L"BUTTON",L"＋ 新增员工",BS_PUSHBUTTON,1000,103,155,29,500);
  simpleList=pageControl(0,WC_LISTVIEWW,L"",WS_BORDER|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,
                         46,172,1110,550,610);
  ListView_SetExtendedListViewStyle(simpleList,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
  SetWindowSubclass(simpleList,coloredListSubclass,1,0);
  const wchar_t* simpleHeaders[]={L"员工",L"等级",L"工作对象",L"推荐工作类型",L"已完成总数"};
  const int simpleWidths[]={230,95,390,220,160};
  for(int i=0;i<5;i++)addColumn(simpleList,i,simpleHeaders[i],simpleWidths[i]);

  availableCombo=pageControl(2,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,46,163,595,300,702);
  containAddButton=pageControl(2,L"BUTTON",L"＋ 添加收容",BS_PUSHBUTTON,657,163,147,29,700);
  containRemoveButton=pageControl(2,L"BUTTON",L"− 移除选中",BS_PUSHBUTTON,817,163,147,29,701);
  containedList=pageControl(2,WC_LISTVIEWW,L"",WS_BORDER|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,
                            46,239,1110,482,703);
  ListView_SetExtendedListViewStyle(containedList,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
  SetWindowSubclass(containedList,coloredListSubclass,1,0);
  addColumn(containedList,0,L"已收容异想体",850);addColumn(containedList,1,L"危险等级",180);

  recommendationList=pageControl(3,WC_LISTVIEWW,L"",WS_BORDER|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,
                                  46,166,1110,389,720);
  ListView_SetExtendedListViewStyle(recommendationList,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
  SetWindowSubclass(recommendationList,coloredListSubclass,1,0);
  const wchar_t* recommendationHeaders[]={L"编号",L"异想体",L"等级",L"收容推荐",L"武器推荐",L"防具推荐",L"镇压"};
  const int recommendationWidths[]={110,166,82,150,245,225,125};
  for(int i=0;i<7;i++)addColumn(recommendationList,i,recommendationHeaders[i],recommendationWidths[i]);
  recommendationDetail=pageControl(3,L"EDIT",L"",WS_BORDER|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY|WS_VSCROLL,
                                    46,570,1110,150,721);

  pageControl(4,L"STATIC",L"搜索",0,46,169,56,26);
  egoSearch=pageControl(4,L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL,105,166,545,29,740);
  SendMessageW(egoSearch,EM_SETCUEBANNER,TRUE,(LPARAM)L"异想体或 E.G.O 名称 / 拼音 / 首字母");
  pageControl(4,L"BUTTON",L"清除",BS_PUSHBUTTON,664,166,78,29,741);
  egoSearchCount=pageControl(4,L"STATIC",L"",SS_RIGHT,925,169,228,26);
  egoList=pageControl(4,WC_LISTVIEWW,L"",WS_BORDER|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,
                      46,211,1110,450,730);
  ListView_SetExtendedListViewStyle(egoList,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_INFOTIP);
  SetWindowSubclass(egoList,coloredListSubclass,1,0);
  const wchar_t* egoHeaders[]={L"编号",L"异想体",L"危险等级",L"E.G.O 名称",L"武器数量",
                              L"武器推荐度",L"装备数量",L"装备推荐度",L"状态"};
  const int egoWidths[]={90,150,75,150,95,165,95,165,95};
  for(int i=0;i<9;i++)addColumn(egoList,i,egoHeaders[i],egoWidths[i]);
  const wchar_t* egoButtonTexts[]={L"武器 −",L"武器 +",L"装备 −",L"装备 +"};
  for(int i=0;i<4;i++)egoButtons[i]=pageControl(4,L"BUTTON",egoButtonTexts[i],BS_PUSHBUTTON,
                                                  46+i*150,680,135,32,710+i);

  loadEmployees();
  loadCollection();
  loadActiveEmployee();
  for(int i=0;i<(int)employees.size();i++)writeEmployee(i);
  rebuildSimpleList();rebuildContainedLists();rebuildRecommendationList();rebuildEgoList();
  for(int page=0;page<PAGE_COUNT;page++)if(page!=currentPage)
    for(HWND item:pageControls[page])ShowWindow(item,SW_HIDE);
}
static int preferenceColumn(HWND child){
  for(int i=0;i<4;i++)for(int j=0;j<5;j++)if(child==rates[i][j])return i;
  return -1;
}
static int attributeIndex(HWND child){
  for(int i=0;i<4;i++)
    if(child==virtueLabels[i]||child==stats[i]||child==targets[i]||child==done[i])return i;
  return -1;
}
static void layoutControls(){
  if(layingOut)return;
  layingOut=true;
  for(int pass=0;pass<3;pass++){
    RECT viewport{};GetClientRect(win,&viewport);
    SCROLLINFO horizontal{sizeof(SCROLLINFO)},vertical{sizeof(SCROLLINFO)};
    horizontal.fMask=vertical.fMask=SIF_RANGE|SIF_PAGE|SIF_POS;
    horizontal.nMin=vertical.nMin=0;
    horizontal.nMax=currentPage==1?DETAIL_SCROLL_WIDTH-1:0;
    vertical.nMax=currentPage==1?DETAIL_SCROLL_HEIGHT-1:0;
    horizontal.nPage=currentPage==1?std::max(1,(int)viewport.right-SIDEBAR_WIDTH):1;
    vertical.nPage=currentPage==1?std::max(1,(int)viewport.bottom):1;
    horizontal.nPos=scrollX;vertical.nPos=scrollY;
    SetScrollInfo(win,SB_HORZ,&horizontal,TRUE);
    SetScrollInfo(win,SB_VERT,&vertical,TRUE);
    RECT adjusted{};GetClientRect(win,&adjusted);
    if(EqualRect(&viewport,&adjusted))break;
  }
  if(currentPage==1){
    SCROLLINFO position{sizeof(SCROLLINFO)};position.fMask=SIF_POS;
    GetScrollInfo(win,SB_HORZ,&position);scrollX=position.nPos;
    GetScrollInfo(win,SB_VERT,&position);scrollY=position.nPos;
  }else scrollX=scrollY=0;
  int dw=extraWidth(),dh=extraHeight();
  RECT client{};GetClientRect(win,&client);
  for(auto& layout:controlLayouts){
    if(layout.page!=currentPage)continue;
    int x=SIDEBAR_WIDTH+layout.x,y=layout.y,w=layout.w,h=layout.h;
    if(layout.page==1){
      detailControlBounds(layout.item,layout.x,layout.y,layout.w,layout.h,x,y,w,h);
      x+=SIDEBAR_WIDTH-scrollX;y-=scrollY;
    }else{
      if(layout.item==simpleList||layout.item==containedList||
         layout.item==recommendationList||layout.item==egoList)w+=dw;
      if(layout.item==recommendationDetail)w+=dw;
      if(layout.item==simpleList||layout.item==containedList||
         layout.item==recommendationList||layout.item==egoList)h+=dh;
      if(layout.item==recommendationDetail||
         std::find(std::begin(egoButtons),std::end(egoButtons),layout.item)!=std::end(egoButtons))y+=dh;
      if(layout.id==500||layout.item==egoSearchCount)x+=dw;
      if(layout.item==availableCombo)w=std::max(260,w+dw);
      if(layout.item==containAddButton)x=SIDEBAR_WIDTH+46+std::max(260,595+dw)+16;
      if(layout.item==containRemoveButton)x=SIDEBAR_WIDTH+46+std::max(260,595+dw)+176;
      if(layout.item==egoSearch)w=std::max(230,w+dw);
      if(layout.id==741)x=SIDEBAR_WIDTH+105+std::max(230,545+dw)+15;
    }
    const bool resized=!layout.placed||layout.lastW!=w||layout.lastH!=h;
    const bool moved=resized||layout.lastX!=x||layout.lastY!=y;
    if(moved&&!SetWindowPos(layout.item,nullptr,x,y,w,h,
                            SWP_NOZORDER|SWP_NOACTIVATE))continue;
    if(layout.page==1){
      const int left=std::clamp(SIDEBAR_WIDTH-x,0,w);
      const int top=std::clamp(-y,0,h);
      const int right=std::clamp((int)client.right-x,left,w);
      const int bottom=std::clamp((int)client.bottom-y,top,h);
      if(!layout.placed||resized||layout.clipLeft!=left||layout.clipTop!=top||
         layout.clipRight!=right||layout.clipBottom!=bottom){
        clipDetailControl(layout.item,layout.framed,layout.list?16:12);
        layout.clipLeft=left;layout.clipTop=top;
        layout.clipRight=right;layout.clipBottom=bottom;
      }
    }else if(layout.framed&&resized)roundControl(layout.item,layout.list?16:12);
    if(layout.list&&resized)roundControl(ListView_GetHeader(layout.item),12);
    layout.lastX=x;layout.lastY=y;layout.lastW=w;layout.lastH=h;
    layout.placed=true;
  }
  layingOut=false;
}
static LRESULT CALLBACK wndProc(HWND h,UINT message,WPARAM wp,LPARAM lp){
  switch(message){
    case WM_CREATE:
      win=h;initialize();SetTimer(h,991,2000,nullptr);return 0;
    case WM_TIMER:
      if(wp==991){
        if(!updateDownloading&&!updateReady)cleanupUpdateFiles(programPath);
        if(++updateCleanupTicks>=5)KillTimer(h,991);
        return 0;
      }
      break;
    case WM_UPDATE_FOUND:{
      std::unique_ptr<ReleaseInfo> release((ReleaseInfo*)lp);
      if(!release)return 0;
      std::wstring prompt=L"发现新版本 "+release->tag+L"（当前 v"+APP_VERSION+
                          L"）。\n\n现在下载更新吗？\n下载失败时会自动尝试国内镜像。";
      if(MessageBoxW(h,prompt.c_str(),L"发现更新",MB_YESNO|MB_ICONINFORMATION)==IDYES){
        updateDownloading=true;InvalidateRect(h,nullptr,FALSE);
        ReleaseInfo chosen=*release;
        try{
          std::thread([h,chosen]{
            auto* result=new UpdateDownloadResult{};
            try{result->success=downloadUpdate(chosen,programPath,result->error);}
            catch(...){result->error=L"下载更新时发生错误。";}
            if(!PostMessageW(h,WM_UPDATE_DOWNLOADED,0,(LPARAM)result))delete result;
          }).detach();
        }catch(...){
          updateDownloading=false;InvalidateRect(h,nullptr,FALSE);
          MessageBoxW(h,L"无法启动下载任务。",L"更新失败",MB_OK|MB_ICONERROR);
        }
      }
      return 0;
    }
    case WM_UPDATE_DOWNLOADED:{
      std::unique_ptr<UpdateDownloadResult> result((UpdateDownloadResult*)lp);
      updateDownloading=false;
      updateReady=result&&result->success;
      InvalidateRect(h,nullptr,FALSE);
      if(updateReady)
        MessageBoxW(h,L"更新已下载并校验完成。程序将在下次启动时自动更新。",
                    L"下载完成",MB_OK|MB_ICONINFORMATION);
      else
        MessageBoxW(h,result?result->error.c_str():L"下载失败。",L"更新失败",
                    MB_OK|MB_ICONERROR);
      return 0;
    }
    case WM_GETMINMAXINFO:{
      RECT bounds{0,0,MIN_WIDTH,MIN_HEIGHT};
      AdjustWindowRect(&bounds,WINDOW_STYLE,FALSE);
      auto* limits=(MINMAXINFO*)lp;
      limits->ptMinTrackSize.x=bounds.right-bounds.left;
      limits->ptMinTrackSize.y=bounds.bottom-bounds.top;
      return 0;
    }
    case WM_ENTERSIZEMOVE:
      resizingWindow=true;resizePending=false;return 0;
    case WM_SIZE:
      if(wp!=SIZE_MINIMIZED){
        if(resizingWindow)resizePending=true;
        else {layoutControls();InvalidateRect(h,nullptr,FALSE);}
      }
      return 0;
    case WM_EXITSIZEMOVE:
      resizingWindow=false;
      if(resizePending){
        resizePending=false;
        layoutControls();
        InvalidateRect(h,nullptr,FALSE);
      }
      return 0;
    case WM_HSCROLL:
    case WM_VSCROLL:{
      if(currentPage!=1||lp)break;
      int bar=message==WM_HSCROLL?SB_HORZ:SB_VERT;
      SCROLLINFO info{sizeof(SCROLLINFO)};info.fMask=SIF_ALL;
      GetScrollInfo(h,bar,&info);
      int position=info.nPos,step=message==WM_HSCROLL?48:46;
      switch(LOWORD(wp)){
        case SB_LINELEFT:position-=step;break;
        case SB_LINERIGHT:position+=step;break;
        case SB_PAGELEFT:position-=(int)info.nPage;break;
        case SB_PAGERIGHT:position+=(int)info.nPage;break;
        case SB_THUMBTRACK:position=info.nTrackPos;break;
        default:return 0;
      }
      info.fMask=SIF_POS;info.nPos=position;
      SetScrollInfo(h,bar,&info,TRUE);
      if(bar==SB_HORZ)scrollX=GetScrollPos(h,bar);
      else scrollY=GetScrollPos(h,bar);
      layoutControls();InvalidateRect(h,nullptr,FALSE);return 0;
    }
    case WM_MOUSEWHEEL:
      if(currentPage==1){
        int steps=GET_WHEEL_DELTA_WPARAM(wp)/WHEEL_DELTA;
        SCROLLINFO info{sizeof(SCROLLINFO)};info.fMask=SIF_POS;
        GetScrollInfo(h,SB_VERT,&info);info.nPos-=steps*90;
        SetScrollInfo(h,SB_VERT,&info,TRUE);
        scrollY=GetScrollPos(h,SB_VERT);
        layoutControls();InvalidateRect(h,nullptr,FALSE);return 0;
      }
      break;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{
      if(resizingWindow&&resizePending){
        PAINTSTRUCT ps{};BeginPaint(h,&ps);EndPaint(h,&ps);return 0;
      }
      PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);
      RECT client{};GetClientRect(h,&client);
      HDC buffer=CreateCompatibleDC(dc);
      HBITMAP bitmap=buffer?CreateCompatibleBitmap(dc,client.right,client.bottom):nullptr;
      if(buffer&&bitmap){
        HGDIOBJ previous=SelectObject(buffer,bitmap);
        paint(buffer);
        BitBlt(dc,0,0,client.right,client.bottom,buffer,0,0,SRCCOPY);
        SelectObject(buffer,previous);
      }else paint(dc);
      if(bitmap)DeleteObject(bitmap);
      if(buffer)DeleteDC(buffer);
      EndPaint(h,&ps);return 0;
    }
    case WM_DRAWITEM:
      if(((const DRAWITEMSTRUCT*)lp)->CtlType==ODT_COMBOBOX)
        paintCombo((const DRAWITEMSTRUCT*)lp);
      else paintButton((const DRAWITEMSTRUCT*)lp);
      return TRUE;
    case WM_MEASUREITEM:
      if(((MEASUREITEMSTRUCT*)lp)->CtlType==ODT_COMBOBOX){
        ((MEASUREITEMSTRUCT*)lp)->itemHeight=27;return TRUE;
      }
      break;
    case WM_LBUTTONDOWN:{
      int x=(short)LOWORD(lp),y=(short)HIWORD(lp);
      if(x>=8&&x<SIDEBAR_WIDTH&&y>=112&&y<112+PAGE_COUNT*64){
        switchPage(sidebarPages[(y-112)/64]);return 0;
      }
      int contentX=x-SIDEBAR_WIDTH+scrollX;
      int contentY=y+scrollY;
      if(currentPage==1&&contentY>=TAB_Y&&contentY<TAB_Y+TAB_HEIGHT){
        if(employees.size()>VISIBLE_TABS&&contentX>=24&&contentX<51&&firstVisibleTab>0)
          firstVisibleTab--;
        else if(employees.size()>VISIBLE_TABS&&contentX>=1040&&contentX<1067&&
                firstVisibleTab+VISIBLE_TABS<(int)employees.size())
          firstVisibleTab++;
        else if(contentX>=TAB_X){
          int column=(contentX-TAB_X)/(TAB_WIDTH+TAB_GAP);
          int offset=(contentX-TAB_X)%(TAB_WIDTH+TAB_GAP);
          int index=firstVisibleTab+column;
          if(offset<TAB_WIDTH&&column<VISIBLE_TABS&&index<(int)employees.size())
            switchEmployee(index);
        }
        InvalidateRect(win,nullptr,FALSE);
        return 0;
      }
      break;
    }
    case WM_CTLCOLORSTATIC:{
      HDC dc=(HDC)wp;HWND child=(HWND)lp;
      int column=preferenceColumn(child);
      if(column>=0){
        SetTextColor(dc,WORK_COLORS[column]);SetBkColor(dc,FIELD);
        return (LRESULT)editBrush;
      }
      int attribute=attributeIndex(child);
      if(attribute>=0){
        SetTextColor(dc,WORK_COLORS[attribute]);SetBkColor(dc,CARD);
        return (LRESULT)cardBrush;
      }
      SetTextColor(dc,child==riskText?RISK_COLORS[CATALOG[selected].level]:MUTED);
      SetBkColor(dc,CARD);
      return (LRESULT)cardBrush;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:{
      HDC dc=(HDC)wp;int column=preferenceColumn((HWND)lp);
      int attribute=attributeIndex((HWND)lp);
      SetTextColor(dc,column>=0?WORK_COLORS[column]:
                        attribute>=0?WORK_COLORS[attribute]:INK);
      SetBkColor(dc,FIELD);return (LRESULT)editBrush;
    }
    case WM_CTLCOLORBTN:{
      HDC dc=(HDC)wp;SetTextColor(dc,INK);SetBkColor(dc,CARD);return (LRESULT)cardBrush;
    }
    case WM_COMMAND:{
      if(HIWORD(wp)==CBN_SELCHANGE&&lp)InvalidateRect((HWND)lp,nullptr,FALSE);
      if(HIWORD(wp)==CBN_DROPDOWN&&lp){
        COMBOBOXINFO info{sizeof(COMBOBOXINFO)};
        if(GetComboBoxInfo((HWND)lp,&info)&&info.hwndList)
          roundControl(info.hwndList,12);
      }
      if(!ready)return 0;
      int id=LOWORD(wp),event=HIWORD(wp);
      if(id==500&&event==BN_CLICKED){
        if(employees.size()>=200)return 0;
        saveState();
        EmployeeState employee{};
        employee.name=L"新员工 "+std::to_wstring(employees.size()+1);
        for(int i=0;i<CATALOG_SIZE;i++)if(contained[i]){employee.abnormality=i;break;}
        employees.push_back(employee);
        activeEmployee=(int)employees.size()-1;
        loadActiveEmployee();
        writeEmployee(activeEmployee);
        rebuildSimpleList();
        if(currentPage==0)switchPage(1);
        return 0;
      }
      if(id==700&&event==BN_CLICKED){
        int choice=(int)SendMessageW(availableCombo,CB_GETCURSEL,0,0);
        if(choice<0)return 0;
        int index=(int)SendMessageW(availableCombo,CB_GETITEMDATA,choice,0);
        contained[index]=true;writeIni(L"containment",CATALOG[index].id,L"1");
        containmentMessage=L"已添加："+std::wstring(CATALOG[index].name);
        rebuildContainedLists();
        ready=false;rebuildWorkChoices();ready=true;
        InvalidateRect(win,nullptr,FALSE);return 0;
      }
      if(id==701&&event==BN_CLICKED){
        int row=ListView_GetNextItem(containedList,-1,LVNI_SELECTED);
        if(row<0){containmentMessage=L"请先在列表中选择异想体。";InvalidateRect(win,nullptr,FALSE);return 0;}
        LVITEMW item{};item.mask=LVIF_PARAM;item.iItem=row;ListView_GetItem(containedList,&item);
        int index=(int)item.lParam;
        if(std::any_of(employees.begin(),employees.end(),[index](const auto& e){return e.abnormality==index;})){
          containmentMessage=L"该异想体仍被员工选作工作对象，请先在详细模式更换。";
          InvalidateRect(win,nullptr,FALSE);return 0;
        }
        if(std::count(contained.begin(),contained.end(),true)<=1){
          containmentMessage=L"至少需要保留一个已收容异想体。";
          InvalidateRect(win,nullptr,FALSE);return 0;
        }
        contained[index]=false;writeIni(L"containment",CATALOG[index].id,L"0");
        containmentMessage=L"已移除："+std::wstring(CATALOG[index].name);
        rebuildContainedLists();
        ready=false;rebuildWorkChoices();ready=true;
        InvalidateRect(win,nullptr,FALSE);return 0;
      }
      if(id>=710&&id<=713&&event==BN_CLICKED){
        int index=selectedEgoIndex();
        if(index<0)return 0;
        bool weapon=id<712;int delta=id%2?1:-1;
        int& owned=weapon?egoWeapons[index]:egoSuits[index];
        int cap=weapon?CATALOG[index].egoWeaponMax:CATALOG[index].egoSuitMax;
        owned=std::clamp(owned+delta,0,cap);
        auto key=std::wstring(CATALOG[index].id)+(weapon?L"_weapon":L"_suit");
        writeIni(L"ego",key.c_str(),std::to_wstring(owned));
        rebuildEgoList();return 0;
      }
      if(id==740&&event==EN_CHANGE){rebuildEgoList();return 0;}
      if(id==741&&event==BN_CLICKED){setText(egoSearch,L"");SetFocus(egoSearch);return 0;}
      if(id==702)return 0;
      if(id>=400&&id<408&&event==BN_CLICKED){
        int i=(id-400)/2;
        int value=std::clamp((int)num(done[i],0)+(id%2?1:-1),0,10000);
        ready=false;setText(done[i],std::to_wstring(value));ready=true;
        saveState();updateOutput();return 0;
      }
      if(id==102&&event==CBN_SELCHANGE){
        saveState();int choice=(int)SendMessageW(abnoBox,CB_GETCURSEL,0,0);
        if(choice<0)return 0;
        selected=(int)SendMessageW(abnoBox,CB_GETITEMDATA,choice,0);
        ready=false;setText(riskText,levels[CATALOG[selected].level]);
        for(HWND item:done)setText(item,L"0");
        fillRates();saveState();updateOutput();return 0;
      }
      if(event==EN_CHANGE||event==CBN_SELCHANGE||event==BN_CLICKED){
        saveState();updateOutput();
      }
      return 0;
    }
    case WM_NOTIFY:{
      if(!ready)return 0;
      auto* notification=(NMHDR*)lp;
      if(notification->code==NM_CUSTOMDRAW&&
         (notification->hwndFrom==ListView_GetHeader(simpleList)||
          notification->hwndFrom==ListView_GetHeader(containedList)||
          notification->hwndFrom==ListView_GetHeader(recommendationList)||
          notification->hwndFrom==ListView_GetHeader(egoList)))
        return paintTableHeader(notification);
      if(notification->code==NM_CUSTOMDRAW&&
         (notification->hwndFrom==simpleList||notification->hwndFrom==containedList||
          notification->hwndFrom==recommendationList||notification->hwndFrom==egoList)){
        auto* paint=(NMLVCUSTOMDRAW*)lp;
        if(paint->nmcd.dwDrawStage==CDDS_PREPAINT)return CDRF_NOTIFYITEMDRAW;
        if(paint->nmcd.dwDrawStage==CDDS_ITEMPREPAINT){
          if(notification->hwndFrom==containedList)return paintContainedRow(paint);
          if(notification->hwndFrom==simpleList)return paintSimpleRow(paint);
          if(notification->hwndFrom==egoList)return paintEgoRow(paint);
          if(notification->hwndFrom==recommendationList)return paintRecommendationRow(paint);
          if(paint->nmcd.uItemState&CDIS_SELECTED)return CDRF_DODEFAULT;
          paint->clrTextBk=(paint->nmcd.dwItemSpec%2)?CARD:ROW;
          paint->clrText=INK;
          return CDRF_NOTIFYSUBITEMDRAW;
        }
        if(paint->nmcd.dwDrawStage==(CDDS_ITEMPREPAINT|CDDS_SUBITEM)){
          int row=(int)paint->nmcd.dwItemSpec;
          if(notification->hwndFrom==simpleList&&paint->iSubItem==3){
            wchar_t work[64]{};ListView_GetItemText(simpleList,row,3,work,64);
            for(int i=0;i<4;i++)if(wcscmp(work,works[i])==0){
              paint->clrText=WORK_COLORS[i];break;
            }
          }
          else if(notification->hwndFrom==recommendationList&&paint->iSubItem==2&&
             row>=0&&row<(int)recommendationOrder.size())
            paint->clrText=RISK_COLORS[CATALOG[recommendationOrder[row]].level];
          return CDRF_DODEFAULT;
        }
        return CDRF_DODEFAULT;
      }
      if(notification->hwndFrom==tabs&&notification->code==TCN_SELCHANGE){
        switchEmployee(TabCtrl_GetCurSel(tabs));return 0;
      }
      if(notification->hwndFrom==simpleList&&notification->code==NM_DBLCLK){
        int row=ListView_GetNextItem(simpleList,-1,LVNI_SELECTED);
        if(row>=0){switchEmployee(row);switchPage(1);}return 0;
      }
      if(notification->hwndFrom==recommendationList&&notification->code==LVN_COLUMNCLICK){
        int column=((NMLISTVIEW*)lp)->iSubItem;
        if(column==0||column==2||column==3){
          if(recommendationSortColumn==column)recommendationDescending=!recommendationDescending;
          else {recommendationSortColumn=column;recommendationDescending=false;}
          rebuildRecommendationList();
        }
        return 0;
      }
      if(notification->hwndFrom==egoList&&notification->code==LVN_COLUMNCLICK){
        int column=((NMLISTVIEW*)lp)->iSubItem;
        if(column==0||column==2){
          if(egoSortColumn==column)egoSortDescending=!egoSortDescending;
          else {egoSortColumn=column;egoSortDescending=false;}
          rebuildEgoList();
        }
        return 0;
      }
      if(notification->hwndFrom==egoList&&notification->code==LVN_GETINFOTIPW){
        auto* tip=(NMLVGETINFOTIPW*)lp;
        if(tip->iItem>=0&&tip->iItem<(int)egoOrder.size()){
          const auto& a=CATALOG[egoOrder[tip->iItem]];
          std::wstring info=L"武器推荐度："+egoRecommendationText(a,true,true)+
                            L"\n装备推荐度："+egoRecommendationText(a,false,true);
          lstrcpynW(tip->pszText,info.c_str(),tip->cchTextMax);
        }
        return 0;
      }
      if(notification->hwndFrom==recommendationList&&notification->code==LVN_ITEMCHANGED){
        auto* item=(NMLISTVIEW*)lp;
        if((item->uChanged&LVIF_STATE)&&(item->uNewState&LVIS_SELECTED))
          if(item->iItem>=0&&item->iItem<(int)recommendationOrder.size())
            showRecommendationDetail(recommendationOrder[item->iItem]);
        return 0;
      }
      if(notification->hwndFrom==egoList&&notification->code==LVN_ITEMCHANGED){
        updateEgoButtons();return 0;
      }
      return 0;
    }
    case WM_CLOSE:saveState();DestroyWindow(h);return 0;
    case WM_DESTROY:
      DeleteObject(bodyFont);DeleteObject(smallFont);DeleteObject(titleFont);
      DeleteObject(sectionFont);DeleteObject(emphasisFont);DeleteObject(headerFont);
      DeleteObject(cardBrush);DeleteObject(editBrush);
      PostQuitMessage(0);return 0;
  }
  return DefWindowProcW(h,message,wp,lp);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show){
  wchar_t path[MAX_PATH]{};
  DWORD pathLength=GetModuleFileNameW(nullptr,path,MAX_PATH);
  if(!pathLength||pathLength>=MAX_PATH)return 1;
  programPath=path;
  int argumentCount=0;
  LPWSTR* arguments=CommandLineToArgvW(GetCommandLineW(),&argumentCount);
  if(arguments&&argumentCount==4&&wcscmp(arguments[1],L"--apply-update")==0){
    std::wstring target=arguments[2];
    unsigned long parent=wcstoul(arguments[3],nullptr,10);
    LocalFree(arguments);
    return runUpdateHelper(programPath,target,parent);
  }
  if(arguments)LocalFree(arguments);
  if(launchPendingUpdate(programPath))return 0;
  SetProcessDPIAware();
  INITCOMMONCONTROLSEX common{sizeof(INITCOMMONCONTROLSEX),ICC_LISTVIEW_CLASSES|ICC_TAB_CLASSES};
  InitCommonControlsEx(&common);
  iniPath=path;
  auto slash=iniPath.find_last_of(L"\\/");iniPath=iniPath.substr(0,slash+1)+L"LobotomyCorpCal.ini";
  WNDCLASSW cls{};cls.lpfnWndProc=wndProc;cls.hInstance=instance;
  cls.lpszClassName=L"LobotomyCorpCalCpp";cls.hCursor=LoadCursor(nullptr,IDC_ARROW);
  cls.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(1));
  RegisterClassW(&cls);
  RECT bounds{0,0,BASE_WIDTH,BASE_HEIGHT};AdjustWindowRect(&bounds,WINDOW_STYLE,FALSE);
  HWND window=CreateWindowExW(0,cls.lpszClassName,L"LobotomyCorpCal · 员工训练计算器",WINDOW_STYLE,
                             CW_USEDEFAULT,CW_USEDEFAULT,bounds.right-bounds.left,
                             bounds.bottom-bounds.top,nullptr,nullptr,instance,nullptr);
  if(!window)return 1;
  ShowWindow(window,show);UpdateWindow(window);
  try{
    std::thread([window]{
      try{
        ReleaseInfo release;
        if(checkForUpdate(release)){
          auto* found=new ReleaseInfo(std::move(release));
          if(!PostMessageW(window,WM_UPDATE_FOUND,0,(LPARAM)found))delete found;
        }
      }catch(...){} // 更新检查失败时仍可离线使用。
    }).detach();
  }catch(...){} // 无法启动后台线程时不影响主程序。
  MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
  return 0;
}
