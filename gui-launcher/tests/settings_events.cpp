#include <windows.h>
#include <objbase.h>
#include <commctrl.h>
#include <windowsx.h>
#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>
#include "ui/settings.h"
#include "resource.h"

using namespace launcher;
namespace {
enum class Mode { Flag, Value, QueryFlag, QueryValue };
struct CliCase { const char* id; const char* flag; Mode mode; Scope scope; const wchar_t* value; };
const CliCase cases[]{
#include "cli_cases.inc"
};
struct OverrideCase { const char* id; const char* key; const wchar_t* value; };
const OverrideCase overrides[]{
#include "override_cases.inc"
};
std::function<void(HWND)> action;
std::string failure;
int count{};
HWND test_owner{};
void require(bool value,const std::string& message) { if(!value)throw std::runtime_error(message); }
void pass(const std::string& name) { ++count;std::cout<<"PASS: "<<name<<'\n'; }
std::wstring text(HWND h) {
  std::wstring s(static_cast<std::size_t>(GetWindowTextLengthW(h))+1,L'\0');
  s.resize(GetWindowTextW(h,s.data(),static_cast<int>(s.size())));return s;
}
HWND control(HWND window,int id) {
  HWND h=GetDlgItem(window,id);if(!h)h=GetDlgItem(GetDlgItem(window,IDC_SETTINGS_PANEL),id);
  require(h!=nullptr,"missing control "+std::to_string(id));return h;
}
void command(HWND window,int id) {SendMessageW(window,WM_COMMAND,id,0);}
void put(HWND window,int id,const std::wstring& value) {SetWindowTextW(control(window,id),value.c_str());}
void check(HWND window,int id,bool on=true) {
  Button_SetCheck(control(window,id),on?BST_CHECKED:BST_UNCHECKED);
  SendMessageW(window,WM_COMMAND,MAKEWPARAM(id,BN_CLICKED),reinterpret_cast<LPARAM>(control(window,id)));
}
void mode(HWND window,int id,int index) {
  SendMessageW(control(window,id),CB_SETCURSEL,index,0);
  SendMessageW(window,WM_COMMAND,MAKEWPARAM(id,CBN_SELCHANGE),reinterpret_cast<LPARAM>(control(window,id)));
}
void accept(HWND window) {
  command(window,IDOK);
  // EndDialog destroys the HWND after the callback returns to the modal loop.
  require(text(control(window,IDC_SETTINGS_PREVIEW)).find(L"请修正以下设置：")==std::wstring::npos,"valid settings rejected");
}
BOOL CALLBACK drive(HWND window,LPARAM) {
  DWORD pid{};GetWindowThreadProcessId(window,&pid);wchar_t name[64]{};GetClassNameW(window,name,64);
  if(pid!=GetCurrentProcessId()||std::wstring(name)!=L"#32770"||!GetDlgItem(window,IDC_SETTINGS_PANEL))return TRUE;
  try { action(window); } catch(const std::exception& e) {failure=e.what();EndDialog(window,IDCANCEL);}
  return FALSE;
}
void CALLBACK timer(HWND,UINT,UINT_PTR id,DWORD) {KillTimer(nullptr,id);EnumWindows(drive,0);}
template<class Fn> bool edit(Session& session,Fn fn) {
  failure.clear();action=fn;SetTimer(nullptr,0,20,timer);
  const bool result=ui::show_settings(test_owner,GetModuleHandleW(nullptr),session);
  require(failure.empty(),failure);return result;
}
template<class Fn> bool edit_right(const Session& session,RightInput& input,Fn fn) {
  failure.clear();action=fn;SetTimer(nullptr,0,20,timer);
  const bool result=ui::show_overrides(test_owner,GetModuleHandleW(nullptr),session,input);
  require(failure.empty(),failure);return result;
}
Session base() {
  auto s=make_session(LR"(D:\fixture)");s.engine_path=LR"(D:\fixture\engine.exe)";
  s.left={InputKind::File,L"left.mp4"};add_right(s,{InputKind::File,L"right.mp4"});return s;
}
std::wstring widen(const char* value) {return {value,value+strlen(value)};}
bool emitted(const Session& s,const char* flag,const wchar_t* value=nullptr) {
  auto result=build_comparison(s);require(result.ok(),"invalid result");
  const auto& args=result.plan->arguments;auto f=std::find(args.begin(),args.end(),widen(flag));
  return f!=args.end()&&(!value||(f+1!=args.end()&&*(f+1)==value));
}
void cli_cases(int first, int last) {
  for(const auto& test:cases) {
    const int number=std::stoi(std::string(test.id).substr(4));
    if(number<first||number>last)continue;
    const auto& catalog=ui::settings_catalog();auto ui=std::find_if(catalog.begin(),catalog.end(),[&](const auto& item){return std::string(item.id)==test.id;});
    require(ui!=catalog.end(),std::string("missing UI mapping ")+test.id);
    if(ui->main_control){pass(std::string(test.id)+" main-window binding (interaction in P03 regression)");continue;}
    auto session=base();const int id=ui::setting_control(test.id);
    const bool query=test.scope==Scope::Query;
    const bool accepted=edit(session,[&](HWND window) {
      const int page=static_cast<int>(ui->page);
      SendMessageW(control(window,IDC_SETTINGS_PAGES),LB_SETCURSEL,page,0);
      SendMessageW(window,WM_COMMAND,MAKEWPARAM(IDC_SETTINGS_PAGES,LBN_SELCHANGE),reinterpret_cast<LPARAM>(control(window,IDC_SETTINGS_PAGES)));
      if(query) {
        if(test.mode==Mode::QueryValue)put(window,id+1,test.value);
        command(window,id);const auto preview=text(control(window,IDC_SETTINGS_PREVIEW));
        require(preview.find(widen(test.flag))!=std::wstring::npos,"query flag missing");
        if(test.mode==Mode::QueryValue)require(preview.find(std::wstring(L"\"")+test.value+L"\"")!=std::wstring::npos,"query value lost");
        command(window,IDCANCEL);return;
      }
      if(std::string(test.id)=="CLI-005") {put(window,IDC_CONFIG_PATH,test.value);command(window,IDC_CONFIG_ADD);}
      else if(std::string(test.id)=="CLI-060") {
        check(window,id,true);command(window,IDC_SETTINGS_VALIDATE);
        require(text(control(window,IDC_SETTINGS_PREVIEW)).find(L"--no-auto-options-file")==std::wstring::npos,"automatic config inversion wrong");
        check(window,id,false);
      } else if(test.mode==Mode::Flag)check(window,id);
      else if(test.scope==Scope::Paired) {
        const std::wstring value=test.value;const auto colon=value.find(L':');
        if(colon==std::wstring::npos){check(window,id);put(window,id+1,value);}
        else {check(window,id+2);put(window,id+3,value.substr(0,colon));check(window,id+4);put(window,id+5,value.substr(colon+1));}
      } else {check(window,id);put(window,id+1,test.value);}
      accept(window);
    });
    require(accepted!=query,"unexpected commit/cancel");
    if(!query)require(emitted(session,test.flag,test.mode==Mode::Value?test.value:nullptr),std::string("wrong GUI emission ")+test.id);
    else require(!emitted(session,test.flag),"query leaked into comparison");
    pass(std::string(test.id)+" GUI value, scope and command");
  }
}
void override_cases() {
  for(const auto& test:overrides) {
    auto session=base();auto input=session.right_inputs[0];const int id=ui::override_control(test.id);
    require(edit_right(session,input,[&](HWND window) {
      require(SendMessageW(control(window,id),CB_GETCOUNT,0,0)==(std::string(test.key)=="filters"?4:3),"unsupported append mode exposed");
      mode(window,id,1);put(window,id+1,test.value);accept(window);
    }),"override not committed");
    session.right_inputs[0]=input;
    auto result=build_comparison(session);require(result.ok(),"override preview invalid");
    require(result.plan->arguments.back().find(L"::"+widen(test.key)+L"="+test.value)!=std::wstring::npos,"override emitted incorrectly");
    require(edit_right(session,input,[&](HWND window){mode(window,id,std::string(test.key)=="filters"?3:2);accept(window);}),"clear failed");
    require(input.overrides.at(test.key).mode==OverrideMode::Clear,"clear lost");
    require(edit_right(session,input,[&](HWND window){mode(window,id,0);accept(window);}),"inherit failed");
    require(input.overrides.find(test.key)==input.overrides.end(),"inherit emits override");
    pass(std::string(test.id)+" replace, clear, inherit and supported modes");
  }
}
void regression_cases() {
  auto session=base();std::wstring error;
  set_option(session,"color-space",std::wstring(L"bt2020nc"),error);
  require(edit(session,[&](HWND window) {
    const int id=ui::setting_control("CLI-024");
    check(window,id+2);put(window,id+3,L"bt709");check(window,id+4);put(window,id+5,L"");accept(window);
  }),"paired edit failed");
  require(session.common.at("color-space")==L"bt2020nc"&&session.right_defaults.at("color-space").empty()&&emitted(session,"--color-space",L"bt709:"),"paired scopes conflated");
  require(edit(session,[&](HWND window){check(window,ui::setting_control("CLI-024")+4,false);accept(window);}),"paired restore failed");
  require(emitted(session,"--color-space",L"bt709:bt2020nc"),"right inheritance not restored");pass("paired common/left/right explicit empty and inheritance");
  const auto before=build_comparison(session).plan->command_line;
  require(!edit(session,[&](HWND window){check(window,ui::setting_control("CLI-004"));command(window,IDCANCEL);}),"cancel committed");
  require(build_comparison(session).plan->command_line==before,"cancel changed session");pass("cancel is transactional across pages");
  require(edit(session,[&](HWND window){accept(window);}),"reopen failed");
  require(build_comparison(session).plan->command_line==before,"reopen changed values");pass("reopen retains values without injecting defaults");
  require(!edit(session,[&](HWND window){
    const int id=ui::setting_control("CLI-021");check(window,id);put(window,id+1,L"0");command(window,IDOK);
    require(IsWindow(window)&&text(control(window,IDC_SETTINGS_PREVIEW)).find(L"frame-buffer-size")!=std::wstring::npos,"invalid value not diagnosed");command(window,IDCANCEL);
  }),"invalid buffer committed");pass("invalid numeric settings remain editable and do not commit");
  require(edit(session,[&](HWND window){
    put(window,IDC_CONFIG_PATH,L"first.opt");command(window,IDC_CONFIG_ADD);put(window,IDC_CONFIG_PATH,L"second.opt");command(window,IDC_CONFIG_ADD);
    command(window,IDC_CONFIG_UP);accept(window);
  }),"config order edit failed");
  require(session.configuration.files==std::vector<std::wstring>{L"second.opt",L"first.opt"},"config order lost");pass("configuration file order preserved");
  require(edit(session,[&](HWND window){
    SendMessageW(control(window,IDC_SETTINGS_PAGES),LB_SETCURSEL,7,0);
    SendMessageW(window,WM_COMMAND,MAKEWPARAM(IDC_SETTINGS_PAGES,LBN_SELCHANGE),reinterpret_cast<LPARAM>(control(window,IDC_SETTINGS_PAGES)));
    command(window,IDC_SETTINGS_RESET);accept(window);
  }),"page reset failed");
  require(session.configuration.files.empty()&&!session.configuration.load_automatic_file&&emitted(session,"--color-space",L"bt709:bt2020nc"),"reset affected another page");pass("reset affects selected page only");
  auto input=session.right_inputs[0];
  require(!edit_right(session,input,[&](HWND window){
    mode(window,ui::override_control("RV-010"),1);put(window,ui::override_control("RV-010")+1,L"-1");command(window,IDOK);
    require(IsWindow(window)&&text(control(window,IDC_SETTINGS_PREVIEW)).find(L"peak-nits")!=std::wstring::npos,"invalid peak accepted");command(window,IDCANCEL);
  }),"invalid override committed");require(input.overrides.empty(),"cancel changed overrides");pass("invalid per-video value does not commit");
  set_option(session,"filters",std::wstring(L"scale=640:-2"),error);
  require(edit_right(session,input,[&](HWND window){mode(window,ui::override_control("RV-001"),2);put(window,ui::override_control("RV-001")+1,L"hflip");accept(window);}),"append failed");
  session.right_inputs[0]=input;auto result=build_comparison(session);
  require(result.ok()&&result.plan->arguments.back().find(L"::filters=__,hflip")!=std::wstring::npos,"append dropped shared template");pass("per-video append retains shared filter template");
}
}
int main(int argc,char** argv) {
  std::cout<<std::unitbuf;CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED|COINIT_DISABLE_OLE1DDE);
  INITCOMMONCONTROLSEX cc{sizeof(cc),ICC_STANDARD_CLASSES|ICC_BAR_CLASSES};InitCommonControlsEx(&cc);
  test_owner=CreateWindowExW(0,L"STATIC",L"Settings test owner",WS_OVERLAPPEDWINDOW,0,0,1080,800,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
  try {
    require(argc==2,"select catalog-a, catalog-b, overrides or semantics");
    const std::string group=argv[1];
    if(group=="catalog-a")cli_cases(1,31);
    else if(group=="catalog-b")cli_cases(32,61);
    else if(group=="overrides")override_cases();
    else if(group=="semantics")regression_cases();
    else throw std::runtime_error("unknown test group");
    std::cout<<"RESULT passed="<<count<<" failed=0\n";
  }
  catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';DestroyWindow(test_owner);CoUninitialize();return 1;}
  DestroyWindow(test_owner);CoUninitialize();return 0;
}
