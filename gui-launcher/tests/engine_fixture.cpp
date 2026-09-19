#include <windows.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

void output(HANDLE stream,const std::string& text){DWORD done{};WriteFile(stream,text.data(),static_cast<DWORD>(text.size()),&done,nullptr);}
LRESULT CALLBACK window_proc(HWND h,UINT m,WPARAM w,LPARAM l){if(m==WM_CLOSE){DestroyWindow(h);return 0;}if(m==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcW(h,m,w,l);}
int wmain(int argc,wchar_t** argv) {
  std::wstring mode=std::filesystem::path(argv[0]).stem().wstring();
  for(int i=1;i+1<argc;++i)if(std::wstring(argv[i])==L"--fixture")mode=argv[i+1];
  std::ofstream record("fixture-argv.bin",std::ios::binary);DWORD count=argc;record.write(reinterpret_cast<const char*>(&count),4);
  for(int i=0;i<argc;++i){DWORD length=static_cast<DWORD>(wcslen(argv[i]));record.write(reinterpret_cast<const char*>(&length),4);record.write(reinterpret_cast<const char*>(argv[i]),length*2);}record.close();
  HANDLE out=GetStdHandle(STD_OUTPUT_HANDLE),err=GetStdHandle(STD_ERROR_HANDLE);
  output(out,"fixture-start\n");output(err,"diagnostic-on-stderr-is-not-necessarily-an-error\n");
  if(mode.find(L"ignore")!=std::wstring::npos||mode.find(L"timeout")!=std::wstring::npos){Sleep(30000);return 0;}
  for(int i=1;i<argc;++i)if(std::wstring(argv[i])==L"--version"){output(out,"video-compare P05 TEST FIXTURE\n");return 0;}
  if(mode.find(L"fail")!=std::wstring::npos){output(err,"fixture-explicit-failure\n");return 23;}
  if(mode.find(L"259")!=std::wstring::npos)return 259;
  if(mode.find(L"wait")!=std::wstring::npos) {
    WNDCLASSW wc{};wc.lpfnWndProc=window_proc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"VideoCompareGUI.Fixture";RegisterClassW(&wc);
    CreateWindowW(wc.lpszClassName,L"P05 fixture",WS_OVERLAPPEDWINDOW,0,0,100,100,nullptr,nullptr,wc.hInstance,nullptr);
    MSG m{};while(GetMessageW(&m,nullptr,0,0)>0){TranslateMessage(&m);DispatchMessageW(&m);}return 0;
  }
  if(mode.find(L"delay")!=std::wstring::npos)Sleep(1500);
  const std::string unicode="\xe4\xb8\xad\xe6\x96\x87\xf0\x9f\x99\x82";
  for(char b:unicode){output(out,std::string(1,b));Sleep(12);}output(out,"\n");
  output(err,std::string("invalid-byte:")+static_cast<char>(0xff)+"\n");
  if(mode.find(L"flood")!=std::wstring::npos||mode.find(L"delay")!=std::wstring::npos) {
    const std::string chunk(16384,'X');
    for(int i=0;i<768;++i){output(out,chunk);output(err,chunk);}
    output(out,"\nFLOOD-END-OUT\n");output(err,"\nFLOOD-END-ERR\n");
  }
  output(out,"fixture-end\n");return 0;
}
