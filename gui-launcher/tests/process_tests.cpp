#include "process/runtime.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace launcher;
using namespace launcher::process;
namespace {
int checks{};
void require(bool v,const char* label){if(!v)throw std::runtime_error(label);++checks;std::cout<<"PASS: "<<label<<'\n';}
std::wstring read(const std::filesystem::path& path){std::ifstream f(path,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(f)),{});Utf8Decoder d;return d.feed(bytes,true);}
Snapshot wait(Run& run,DWORD timeout=15000) {
  const auto started=GetTickCount64();
  while(run.active()&&GetTickCount64()-started<timeout)Sleep(20);
  if(run.active()){run.force_stop();Sleep(500);throw std::runtime_error("fixture timed out");}return run.poll();
}
LaunchPlan plan(const std::filesystem::path& exe,const std::filesystem::path& work,const std::vector<std::wstring>& extra) {
  LaunchPlan p;p.executable=exe.wstring();p.working_directory=work.wstring();p.arguments={p.executable};p.arguments.insert(p.arguments.end(),extra.begin(),extra.end());p.command_line=make_windows_command_line(p.arguments);p.preview=p.command_line;return p;
}
std::vector<std::wstring> arguments(const std::filesystem::path& path) {
  std::ifstream f(path,std::ios::binary);DWORD n{};f.read(reinterpret_cast<char*>(&n),4);if(n>16384)throw std::runtime_error("bad argv count");std::vector<std::wstring> result;
  while(n--){DWORD length{};f.read(reinterpret_cast<char*>(&length),4);if(length>32767)throw std::runtime_error("bad argv length");std::wstring s(length,L'\0');f.read(reinterpret_cast<char*>(s.data()),length*2);result.push_back(s);}return result;
}
}
int wmain(int argc,wchar_t** argv) {
  std::cout<<std::unitbuf;if(argc!=2)return 2;
  const std::filesystem::path root=std::filesystem::path(argv[1])/std::to_wstring(GetCurrentProcessId());std::filesystem::create_directories(root);
  const auto bin=std::filesystem::path(executable_path()).parent_path();const auto fixture=bin/L"launcher-engine-fixture.exe";
  try {
    Utf8Decoder decoder;std::wstring decoded;const std::string bytes="\xe4\xb8\xad\xe6\x96\x87\xf0\x9f\x99\x82";
    for(char c:bytes)decoded+=decoder.feed(std::string(1,c));decoded+=decoder.feed({},true);
    require(decoded==L"中文🙂","UTF-8 survives arbitrary chunk boundaries");
    require(decoder.feed(std::string(1,static_cast<char>(0xff)),true)==L"\xfffd","invalid UTF-8 has visible replacement");
    std::wstring error;Run run;
    auto special=root/L"中文 & (引擎)#";std::filesystem::create_directories(special);const auto copied=special/L"video compare.exe";std::filesystem::copy_file(fixture,copied,std::filesystem::copy_options::overwrite_existing);
    auto p=plan(copied,root,{L"",L"中文 & (片段)#.mp4",L"quote\"inside",L"ends\\",L"C:\\space path\\",L"filter=\"x:y\"::filters=scale=640:-2",L"--fixture",L"normal"});
    require(run.start(p,(root/L"runs").wstring(),0,error),"Unicode executable and working directory launch");
    require(!run.start(p,(root/L"runs").wstring(),0,error),"duplicate start rejected");
    auto done=wait(run);require(done.phase==Phase::Exited&&done.exit_code==0,"normal exit and status");
    require(arguments(root/L"fixture-argv.bin")==p.arguments,"child argv exactly matches array including empty quotes and Unicode");
    require(read(std::filesystem::path(run.folder())/L"stdout.raw").find(L"中文🙂")!=std::wstring::npos,"raw stdout preserves Unicode bytes");
    require(done.tail.find(L"stderr")!=std::wstring::npos&&done.tail.find(L"stdout")!=std::wstring::npos,"stream sources tagged without treating stderr as failure");
    require(run.start(plan(fixture,root,{L"--fixture",L"fail"}),(root/L"runs").wstring(),0,error),"retry after completed task");
    done=wait(run);require(done.exit_code==23&&done.tail.find(L"fixture-explicit-failure")!=std::wstring::npos,"nonzero exit preserves stderr");
    require(run.start(plan(fixture,root,{L"--fixture",L"259"}),(root/L"runs").wstring(),0,error),"start exit-259 fixture");
    require(wait(run).exit_code==259,"exit 259 is not mistaken for a live process");
    require(run.start(plan(fixture,root,{L"--fixture",L"flood"}),(root/L"runs").wstring(),0,error),"start simultaneous stream flood");
    done=wait(run);require(done.exit_code==0&&done.tail.find(L"FLOOD-END-OUT")!=std::wstring::npos&&done.tail.find(L"FLOOD-END-ERR")!=std::wstring::npos,"both streams drain through long lines and flood");
    for(const auto* name:{L"stdout.raw",L"stdout.raw.1",L"stderr.raw",L"stderr.raw.1"})require(std::filesystem::file_size(std::filesystem::path(run.folder())/name)<=4*1024*1024,"raw stream rotation bounded at 4 MiB per file");
    require(done.tail.size()<=65536,"UI tail is bounded");
    require(run.start(plan(fixture,root,{L"--fixture",L"wait"}),(root/L"runs").wstring(),0,error),"start cooperative stop fixture");Sleep(200);run.request_stop();
    require(wait(run).exit_code==0,"normal stop closes only the owned process window");
    require(run.start(plan(fixture,root,{L"--fixture",L"ignore"}),(root/L"runs").wstring(),0,error),"start uncooperative stop fixture");Sleep(100);run.request_stop();Sleep(5300);
    require(run.poll().phase==Phase::StopTimedOut&&run.active(),"normal stop timeout leaves force choice to user");run.force_stop();
    require(wait(run).exit_code==0xc000013a,"explicit force ends owned process");
    require(run.start(plan(fixture,root,{L"--fixture",L"timeout"}),(root/L"runs").wstring(),200,error),"start timed probe fixture");
    require(wait(run).message.find(L"检查超时")!=std::wstring::npos,"probe deadline enforced independently of GUI");
    auto missing=root/L"missing-dll";std::filesystem::create_directories(missing);std::filesystem::copy_file(bin/L"launcher-missing-dll-fixture.exe",missing/L"engine.exe",std::filesystem::copy_options::overwrite_existing);
    require(run.start(plan(missing/L"engine.exe",root,{}),(root/L"runs").wstring(),3000,error),"start missing-DLL loader fixture");
    done=wait(run);require(done.exit_code==0xc0000135&&done.message.find(L"DLL")!=std::wstring::npos,"real missing-DLL failure diagnosed");
    require(!run.start(plan(root/L"absent.exe",root,{}),(root/L"runs").wstring(),0,error),"missing engine rejected before launch");
    {std::ofstream f(root/L"bad.exe");f<<"not a PE";}
    require(!run.start(plan(root/L"bad.exe",root,{}),(root/L"runs").wstring(),0,error),"invalid EXE rejected before launch");
    require(!run.start(plan(fixture,root/L"absent-directory",{}),(root/L"runs").wstring(),0,error),"missing working directory rejected");
    std::wstring detached_folder;
    {Run detached;require(detached.start(plan(fixture,root,{L"--fixture",L"delay-flood"}),(root/L"runs").wstring(),0,error),"start detached logging fixture");detached_folder=detached.folder();}
    const auto deadline=GetTickCount64()+15000;
    while(GetTickCount64()<deadline&&read(std::filesystem::path(detached_folder)/L"events.log").find(L"退出码=0")==std::wstring::npos)Sleep(30);
    require(read(std::filesystem::path(detached_folder)/L"tail.txt").find(L"FLOOD-END-ERR")!=std::wstring::npos,"closing controller leaves helper draining to engine exit");
    DWORD before{},after{};GetProcessHandleCount(GetCurrentProcess(),&before);
    for(int i=0;i<10;++i){require(run.start(plan(fixture,root,{L"--version"}),(root/L"runs").wstring(),1000,error),"repeat launch");require(wait(run).exit_code==0,"repeat completion");}
    GetProcessHandleCount(GetCurrentProcess(),&after);require(after<=before+2,"ten runs do not accumulate process or event handles");
    std::cout<<"RESULT passed="<<checks<<" failed=0\n";
  }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}return 0;
}
