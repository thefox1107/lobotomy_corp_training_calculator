#include <windows.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdlib>
#include "catalog.hpp"
#include "core.hpp"

static constexpr COLORREF BG=RGB(241,239,234), CARD=RGB(255,255,255);
static constexpr COLORREF DARK=RGB(34,42,46), INK=RGB(42,48,50);
static constexpr COLORREF MUTED=RGB(104,112,113), BORDER=RGB(222,225,223);
static constexpr COLORREF RED=RGB(170,68,58), SOFT_RED=RGB(250,238,235);
static constexpr COLORREF GREEN=RGB(67,123,99), ROW=RGB(248,249,247);
static HWND win,abnoBox,nameEdit,riskText,capBox,stats[4],targets[4],done[4];
static HWND techBox,residentBox,clerksBox,boxesEdit,hpEdit,spEdit,rates[4][5];
static HFONT bodyFont,smallFont,titleFont,sectionFont,emphasisFont;
static HBRUSH cardBrush,editBrush;
static std::wstring iniPath,summary,detail;
static PlanInput viewPlan;
static WorkResult viewResults[4];
static int selected=1,best=-1,missing=0;
static bool ready=false;
static const wchar_t* works[]={L"本能",L"洞察",L"沟通",L"压迫"};
static const wchar_t* virtues[]={L"勇气",L"谨慎",L"自律",L"正义"};
static const wchar_t* levels[]={L"ZAYIN",L"TETH",L"HE",L"WAW",L"ALEPH"};
static const double expectedBoxes[]={5,8,12,18,23};

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
static HWND control(const wchar_t* cls,const wchar_t* value,DWORD style,int x,int y,int w,int h,int id=0){
  HWND item=CreateWindowExW(0,cls,value,WS_CHILD|WS_VISIBLE|style,x,y,w,h,win,
                           (HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);
  SendMessageW(item,WM_SETFONT,(WPARAM)bodyFont,TRUE);return item;
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
  p.boxes=std::clamp(num(boxesEdit,expectedBoxes[p.level]),0.,40.);
  p.hp=std::clamp(num(hpEdit,100.)/100,0.,1.);
  p.sp=std::clamp(num(spEdit,100.)/100,0.,1.);
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
  viewPlan=readPlan();best=-1;missing=0;
  if(!CATALOG[selected].regularWork){
    summary=L"此对象没有常规工作";
    detail=L"工具类或无法收容的对象不参与训练推荐。";
  }else{
    int bestRate=-1;
    for(int i=0;i<4;i++){
      viewResults[i]=simulate(viewPlan,i);
      auto r=viewResults[i];
      if(r.rate==-101)missing++;
      if(r.rate>0&&r.remaining>0&&r.rate>bestRate){best=i;bestRate=r.rate;}
    }
    if(best>=0){
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
  RECT area{648,372,1178,699};InvalidateRect(win,&area,FALSE);
}
static void saveState(){
  if(!ready)return;
  writeIni(L"plan",L"name",getText(nameEdit));writeIni(L"plan",L"abno",CATALOG[selected].id);
  for(int i=0;i<4;i++){
    auto n=std::to_wstring(i);
    writeIni(L"plan",(L"current"+n).c_str(),getText(stats[i]));
    writeIni(L"plan",(L"target"+n).c_str(),getText(targets[i]));
    writeIni(L"plan",(L"done"+n).c_str(),getText(done[i]));
  }
  writeIni(L"plan",L"tech",SendMessageW(techBox,BM_GETCHECK,0,0)==BST_CHECKED?L"1":L"0");
  writeIni(L"plan",L"resident",std::to_wstring(SendMessageW(residentBox,CB_GETCURSEL,0,0)));
  writeIni(L"plan",L"clerks",std::to_wstring(SendMessageW(clerksBox,CB_GETCURSEL,0,0)));
  writeIni(L"plan",L"cap",std::to_wstring(SendMessageW(capBox,CB_GETCURSEL,0,0)));
  writeIni(L"plan",L"boxes",getText(boxesEdit));
  writeIni(L"plan",L"hp",getText(hpEdit));writeIni(L"plan",L"sp",getText(spEdit));
  if(CATALOG[selected].rates[0][0]==-101&&CATALOG[selected].regularWork){
    auto section=prefSection();
    for(int i=0;i<4;i++)for(int j=0;j<5;j++){
      auto key=L"r"+std::to_wstring(i)+L"_"+std::to_wstring(j);
      writeIni(section.c_str(),key.c_str(),getText(rates[i][j]));
    }
  }
}
static void fill(HDC dc,int x,int y,int w,int h,COLORREF color){
  RECT r{x,y,x+w,y+h};HBRUSH b=CreateSolidBrush(color);FillRect(dc,&r,b);DeleteObject(b);
}
static void rounded(HDC dc,int x,int y,int w,int h,COLORREF color){
  HBRUSH b=CreateSolidBrush(color);HPEN p=CreatePen(PS_SOLID,1,BORDER);
  auto oldB=SelectObject(dc,b);auto oldP=SelectObject(dc,p);
  RoundRect(dc,x,y,x+w,y+h,18,18);
  SelectObject(dc,oldB);SelectObject(dc,oldP);DeleteObject(b);DeleteObject(p);
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
static void paintResults(HDC dc){
  rounded(dc,670,407,486,69,SOFT_RED);
  fill(dc,670,419,4,45,RED);
  draw(dc,summary,688,412,450,32,emphasisFont,RED);
  draw(dc,detail,688,446,450,23,smallFont,INK);
  if(!CATALOG[selected].regularWork){
    draw(dc,L"请选择可进行常规工作的异想体。",674,526,475,32,bodyFont,MUTED);
    return;
  }
  draw(dc,L"工作",674,489,65,22,smallFont,MUTED);
  draw(dc,L"属性",751,489,60,22,smallFont,MUTED);
  draw(dc,L"偏好",816,489,66,22,smallFont,MUTED);
  draw(dc,L"单次增益",892,489,88,22,smallFont,MUTED);
  draw(dc,L"计划 / 已做 / 剩余",992,489,163,22,smallFont,MUTED);
  for(int i=0;i<4;i++){
    int y=518+i*35;auto r=viewResults[i];
    fill(dc,670,y,486,32,i==best?SOFT_RED:(i%2?CARD:ROW));
    if(i==best)fill(dc,670,y,3,32,RED);
    draw(dc,works[i],677,y,60,32,bodyFont,i==best?RED:INK);
    draw(dc,virtues[i],751,y,60,32,bodyFont,INK);
    draw(dc,r.rate==-101?L"—":std::to_wstring(r.rate)+L"%",816,y,70,32,bodyFont,
         r.rate>0?GREEN:MUTED);
    draw(dc,r.rate==-101?L"—":fmt(r.firstGain),892,y,88,32,bodyFont,INK);
    std::wstring count=r.rate==-101?L"—":std::to_wstring(r.count)+L" / "+
                       std::to_wstring(viewPlan.completed[i])+L" / "+std::to_wstring(r.remaining);
    draw(dc,count,992,y,163,32,bodyFont,INK);
  }
  std::wstring note=viewPlan.level==3?L"WAW 连续工作过载未计入估算。":
                    L"估算次数会随当前属性与训练条件更新。";
  draw(dc,note,672,664,482,25,smallFont,MUTED);
}
static void paint(HDC dc){
  RECT whole{0,0,1200,720};HBRUSH bg=CreateSolidBrush(BG);FillRect(dc,&whole,bg);DeleteObject(bg);
  fill(dc,0,0,1200,88,DARK);
  fill(dc,23,24,42,40,RED);
  draw(dc,L"LC",28,26,32,34,sectionFont,CARD,DT_CENTER);
  draw(dc,L"员工训练计算器",79,16,580,43,titleFont,CARD);
  draw(dc,L"LOBOTOMY CORPORATION   /   TRAINING PLANNER",82,57,650,20,smallFont,RGB(183,190,187));
  draw(dc,L"离线桌面版  ·  自动保存",850,35,318,25,smallFont,RGB(207,211,208),DT_RIGHT);
  rounded(dc,22,104,608,594,CARD);
  rounded(dc,648,104,530,594,CARD);
  section(dc,L"员工档案",45,122,540);
  line(dc,46,266,605,266,BORDER);
  section(dc,L"属性与目标",45,270,540);
  line(dc,46,505,605,505,BORDER);
  section(dc,L"训练条件",45,509,540);
  section(dc,L"工作偏好",671,122,470);
  draw(dc,L"按员工品质 I–V 的基础成功率 %",671,151,470,22,smallFont,MUTED);
  line(dc,672,373,1154,373,BORDER);
  section(dc,L"训练结果",671,378,470);
}
static void initialize(){
  ready=false;
  bodyFont=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
  smallFont=CreateFontW(-13,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
  titleFont=CreateFontW(-31,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
  sectionFont=CreateFontW(-19,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
  emphasisFont=CreateFontW(-22,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
  cardBrush=CreateSolidBrush(CARD);editBrush=CreateSolidBrush(CARD);
  label(L"员工姓名",47,157,195);nameEdit=edit(47,183,221,101);
  label(L"工作异想体",286,157,322);
  abnoBox=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,286,183,319,300,102);
  for(int i=0;i<CATALOG_SIZE;i++){
    std::wstring name=std::wstring(CATALOG[i].id)+L"  "+CATALOG[i].name;
    SendMessageW(abnoBox,CB_ADDSTRING,0,(LPARAM)name.c_str());
  }
  label(L"危险等级",47,226,100);
  riskText=label(L"",153,226,115,26);
  SendMessageW(riskText,WM_SETFONT,(WPARAM)sectionFont,TRUE);
  label(L"属性上限",286,226,100);
  capBox=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST,389,224,216,120,146);
  for(const wchar_t* value:{L"100  常规",L"120  记录部研究",L"130  核心抑制"})
    SendMessageW(capBox,CB_ADDSTRING,0,(LPARAM)value);
  for(int i=0;i<4;i++){
    int x=47+i*140;
    label(virtues[i],x,307,120);
    label(L"当前",x,329,120);stats[i]=edit(x,349,114,110+i);
    label(L"目标",x,387,120);targets[i]=edit(x,407,114,120+i);
    label(L"已完成",x,445,120);done[i]=edit(x,465,114,130+i);
  }
  techBox=control(L"BUTTON",L"普及培训手册  +0.5",BS_AUTOCHECKBOX,47,543,270,26,140);
  label(L"培训部常驻",47,574,145);residentBox=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST,47,597,164,120,141);
  label(L"文职加成",227,574,140);clerksBox=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST,227,597,164,120,142);
  for(const wchar_t* value:{L"无",L"1级",L"2级",L"3级"}){
    SendMessageW(residentBox,CB_ADDSTRING,0,(LPARAM)value);
    SendMessageW(clerksBox,CB_ADDSTRING,0,(LPARAM)value);
  }
  label(L"预计 PE-BOX",407,574,198);boxesEdit=edit(407,597,198,143);
  label(L"结束生命 %",47,639,145);hpEdit=edit(164,638,95,144);
  label(L"结束精神 %",290,639,145);spEdit=edit(412,638,95,145);
  for(int j=0;j<5;j++){
    std::wstring text=L"品质 "+std::to_wstring(j+1);
    label(text.c_str(),735+j*82,183,76);
  }
  for(int i=0;i<4;i++){
    label(works[i],672,218+i*37,59,29);
    for(int j=0;j<5;j++)rates[i][j]=edit(735+j*82,217+i*37,69,200+i*5+j);
  }
  std::wstring id=readIni(L"plan",L"abno",L"O-03-03");
  for(int i=0;i<CATALOG_SIZE;i++)if(id==CATALOG[i].id){selected=i;break;}
  SendMessageW(abnoBox,CB_SETCURSEL,selected,0);
  setText(nameEdit,readIni(L"plan",L"name",L"新员工"));
  for(int i=0;i<4;i++){
    auto n=std::to_wstring(i);
    setText(stats[i],readIni(L"plan",(L"current"+n).c_str(),L"20"));
    setText(targets[i],readIni(L"plan",(L"target"+n).c_str(),L"30"));
    setText(done[i],readIni(L"plan",(L"done"+n).c_str(),L"0"));
  }
  SendMessageW(techBox,BM_SETCHECK,readIni(L"plan",L"tech",L"0")==L"1"?BST_CHECKED:BST_UNCHECKED,0);
  SendMessageW(residentBox,CB_SETCURSEL,std::clamp(_wtoi(readIni(L"plan",L"resident",L"0").c_str()),0,3),0);
  SendMessageW(clerksBox,CB_SETCURSEL,std::clamp(_wtoi(readIni(L"plan",L"clerks",L"0").c_str()),0,3),0);
  SendMessageW(capBox,CB_SETCURSEL,std::clamp(_wtoi(readIni(L"plan",L"cap",L"0").c_str()),0,2),0);
  auto boxDefault=std::to_wstring((int)expectedBoxes[CATALOG[selected].level]);
  setText(boxesEdit,readIni(L"plan",L"boxes",boxDefault.c_str()));
  setText(hpEdit,readIni(L"plan",L"hp",L"100"));
  setText(spEdit,readIni(L"plan",L"sp",L"100"));
  setText(riskText,levels[CATALOG[selected].level]);
  fillRates();updateOutput();
}
static LRESULT CALLBACK wndProc(HWND h,UINT message,WPARAM wp,LPARAM lp){
  switch(message){
    case WM_CREATE:win=h;initialize();return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{
      PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);
      paint(dc);if(ready)paintResults(dc);
      EndPaint(h,&ps);return 0;
    }
    case WM_CTLCOLORSTATIC:{
      HDC dc=(HDC)wp;HWND child=(HWND)lp;
      SetTextColor(dc,child==riskText?RED:MUTED);
      SetBkColor(dc,CARD);return (LRESULT)cardBrush;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:{
      HDC dc=(HDC)wp;SetTextColor(dc,INK);SetBkColor(dc,CARD);return (LRESULT)editBrush;
    }
    case WM_CTLCOLORBTN:{
      HDC dc=(HDC)wp;SetTextColor(dc,INK);SetBkColor(dc,CARD);return (LRESULT)cardBrush;
    }
    case WM_COMMAND:{
      if(!ready)return 0;
      int id=LOWORD(wp),event=HIWORD(wp);
      if(id==102&&event==CBN_SELCHANGE){
        saveState();selected=(int)SendMessageW(abnoBox,CB_GETCURSEL,0,0);
        ready=false;setText(riskText,levels[CATALOG[selected].level]);
        setText(boxesEdit,std::to_wstring((int)expectedBoxes[CATALOG[selected].level]));
        for(HWND item:done)setText(item,L"0");
        fillRates();saveState();updateOutput();return 0;
      }
      if(event==EN_CHANGE||event==CBN_SELCHANGE||event==BN_CLICKED){
        saveState();updateOutput();
      }
      return 0;
    }
    case WM_CLOSE:saveState();DestroyWindow(h);return 0;
    case WM_DESTROY:
      DeleteObject(bodyFont);DeleteObject(smallFont);DeleteObject(titleFont);
      DeleteObject(sectionFont);DeleteObject(emphasisFont);
      DeleteObject(cardBrush);DeleteObject(editBrush);
      PostQuitMessage(0);return 0;
  }
  return DefWindowProcW(h,message,wp,lp);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show){
  SetProcessDPIAware();
  wchar_t path[MAX_PATH]{};GetModuleFileNameW(nullptr,path,MAX_PATH);iniPath=path;
  auto slash=iniPath.find_last_of(L"\\/");iniPath=iniPath.substr(0,slash+1)+L"LobotomyCorpCal.ini";
  WNDCLASSW cls{};cls.lpfnWndProc=wndProc;cls.hInstance=instance;
  cls.lpszClassName=L"LobotomyCorpCalCpp";cls.hCursor=LoadCursor(nullptr,IDC_ARROW);
  cls.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(1));
  RegisterClassW(&cls);
  DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_CLIPCHILDREN;
  RECT bounds{0,0,1200,720};AdjustWindowRect(&bounds,style,FALSE);
  HWND window=CreateWindowExW(0,cls.lpszClassName,L"LobotomyCorpCal · 员工训练计算器",style,
                             CW_USEDEFAULT,CW_USEDEFAULT,bounds.right-bounds.left,
                             bounds.bottom-bounds.top,nullptr,nullptr,instance,nullptr);
  if(!window)return 1;
  ShowWindow(window,show);UpdateWindow(window);
  MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
  return 0;
}
