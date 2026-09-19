#include "runtime.h"
#include <objbase.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <stdexcept>

namespace launcher::process {
namespace {
constexpr DWORD kLogLimit = 4 * 1024 * 1024;
constexpr std::size_t kTailLimit = 65536;
struct Handle {
  HANDLE value{};
  Handle() = default;
  explicit Handle(HANDLE h):value(h) {}
  ~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
  Handle(const Handle&)=delete;
  Handle& operator=(const Handle&)=delete;
  void reset(HANDLE h=nullptr){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);value=h;}
};
std::wstring join(const std::wstring& a,const wchar_t* b){return (std::filesystem::path(a)/b).wstring();}
std::wstring stamp() {
  SYSTEMTIME t{};GetSystemTime(&t);wchar_t text[80]{};
  swprintf_s(text,L"%04u-%02u-%02u %02u:%02u:%02u.%03u UTC",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds);
  return text;
}
std::wstring token(const std::wstring& folder){return std::filesystem::path(folder).filename().wstring();}
std::wstring event_name(const std::wstring& folder,const wchar_t* suffix){return L"Local\\VideoCompareGUI-"+token(folder)+suffix;}
void write_all(HANDLE file,const char* data,std::size_t size) {
  while(size){DWORD done{};const DWORD part=static_cast<DWORD>(std::min<std::size_t>(size,65536));
    if(!WriteFile(file,data,part,&done,nullptr)||!done)throw std::runtime_error("Log write failed");data+=done;size-=done;}
}
void write_file(const std::wstring& path,const std::string& bytes,bool atomic=false) {
  const auto target=atomic?path+L".tmp":path;
  Handle file(CreateFileW(target.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr));
  if(file.value==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create log file");
  write_all(file.value,bytes.data(),bytes.size());file.reset();
  if(atomic&&!MoveFileExW(target.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING))throw std::runtime_error("Cannot update log snapshot");
}
std::string read_file(const std::wstring& path,std::size_t limit) {
  Handle file(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
  if(file.value==INVALID_HANDLE_VALUE)return {};
  LARGE_INTEGER length{};if(!GetFileSizeEx(file.value,&length)||length.QuadPart<0||static_cast<ULONGLONG>(length.QuadPart)>limit)return {};
  std::string bytes(static_cast<std::size_t>(length.QuadPart),'\0');DWORD done{};
  if(!bytes.empty()&&(!ReadFile(file.value,bytes.data(),static_cast<DWORD>(bytes.size()),&done,nullptr)||done!=bytes.size()))return {};
  return bytes;
}
void number(std::string& data,DWORD value){data.append(reinterpret_cast<const char*>(&value),sizeof(value));}
void string(std::string& data,const std::wstring& value){number(data,static_cast<DWORD>(value.size()));data.append(reinterpret_cast<const char*>(value.data()),value.size()*sizeof(wchar_t));}
struct Reader {
  const std::string& bytes;std::size_t offset{};
  DWORD number(){if(offset+4>bytes.size())throw std::runtime_error("Invalid request");DWORD v{};memcpy(&v,bytes.data()+offset,4);offset+=4;return v;}
  std::wstring string(){auto size=number();if(size>1048576||offset+size*2>bytes.size())throw std::runtime_error("Invalid string");std::wstring s(size,L'\0');memcpy(s.data(),bytes.data()+offset,size*2);offset+=size*2;return s;}
};
void save_status(const std::wstring& folder,const Status& status) {
  std::string bytes;number(bytes,static_cast<DWORD>(status.phase));number(bytes,status.pid);number(bytes,status.exit_code);string(bytes,status.message);
  write_file(join(folder,L"state.bin"),bytes,true);
  Handle events(CreateFileW(join(folder,L"events.log").c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr));
  if(events.value!=INVALID_HANDLE_VALUE){const auto line=utf8(stamp()+L" pid="+std::to_wstring(status.pid)+L" exit="+std::to_wstring(status.exit_code)+L" "+status.message+L"\r\n");write_all(events.value,line.data(),line.size());}
}
struct RawLog {
  std::wstring path;Handle file;DWORD size{};bool failed{};
  explicit RawLog(std::wstring name):path(std::move(name)){open();}
  void open(){file.reset(CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr));if(file.value==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create stream log");size=0;}
  void append(const std::string& bytes) {
    if(failed)return;
    try {
      std::size_t offset=0;
      while(offset<bytes.size()) {
        if(size==kLogLimit){file.reset();if(!MoveFileExW(path.c_str(),(path+L".1").c_str(),MOVEFILE_REPLACE_EXISTING))throw std::runtime_error("Cannot rotate log");open();}
        const auto n=std::min<std::size_t>(bytes.size()-offset,kLogLimit-size);write_all(file.value,bytes.data()+offset,n);size+=static_cast<DWORD>(n);offset+=n;
      }
    }catch(...){failed=true;file.reset();}
  }
};
struct Tail {
  std::wstring value;
  void append(const std::wstring& source,const std::wstring& text) {
    if(text.empty())return;
    value+=L"["+source+L"] "+text;
    if(value.empty()||value.back()!=L'\n')value+=L"\r\n";
    if(value.size()>kTailLimit) {value.erase(0,value.size()-kTailLimit);if(!value.empty()&&value[0]>=0xdc00&&value[0]<=0xdfff)value.erase(0,1);}
  }
};
BOOL CALLBACK close_window(HWND window,LPARAM param) {
  DWORD pid{};GetWindowThreadProcessId(window,&pid);if(pid==static_cast<DWORD>(param))PostMessageW(window,WM_CLOSE,0,0);return TRUE;
}
struct Pipe {
  Handle read,write;
  Pipe(){SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};if(!CreatePipe(&read.value,&write.value,&sa,0)||!SetHandleInformation(read.value,HANDLE_FLAG_INHERIT,0))throw std::runtime_error("Cannot create pipe");}
};
bool drain(Pipe& pipe,RawLog& log,Utf8Decoder& decoder,Tail& tail,const wchar_t* source) {
  if(!pipe.read.value)return false;
  // Fair, bounded batches prevent a noisy stream starving the other stream or stop events.
  for(int batch=0;batch<8;++batch) {
    DWORD available{};
    if(!PeekNamedPipe(pipe.read.value,nullptr,0,nullptr,&available,nullptr)){tail.append(source,decoder.feed({},true));pipe.read.reset();return false;}
    if(!available)return true;
    std::string bytes(std::min<DWORD>(available,16384),'\0');DWORD count{};
    if(!ReadFile(pipe.read.value,bytes.data(),static_cast<DWORD>(bytes.size()),&count,nullptr)||!count){tail.append(source,decoder.feed({},true));pipe.read.reset();return false;}
    bytes.resize(count);log.append(bytes);tail.append(source,decoder.feed(bytes));
  }
  return true;
}
struct Attributes {
  std::vector<char> bytes;LPPROC_THREAD_ATTRIBUTE_LIST list{};
  Attributes(HANDLE* handles,std::size_t count) {
    SIZE_T size{};InitializeProcThreadAttributeList(nullptr,1,0,&size);bytes.resize(size);list=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(bytes.data());
    if(!InitializeProcThreadAttributeList(list,1,0,&size)){list=nullptr;throw std::runtime_error("Cannot initialize handle whitelist");}
    if(!UpdateProcThreadAttribute(list,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,handles,count*sizeof(HANDLE),nullptr,nullptr)){DeleteProcThreadAttributeList(list);list=nullptr;throw std::runtime_error("Cannot set handle whitelist");}
  }
  ~Attributes(){if(list)DeleteProcThreadAttributeList(list);}
};
}
std::wstring system_error(DWORD code) {
  PWSTR text{};const auto n=FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,nullptr,code,0,reinterpret_cast<PWSTR>(&text),0,nullptr);
  std::wstring result=n?std::wstring(text,n):L"未知系统错误";if(text)LocalFree(text);
  while(!result.empty()&&(result.back()==L'\r'||result.back()==L'\n'))result.pop_back();return result+L" ("+std::to_wstring(code)+L")";
}
std::wstring exit_description(DWORD code) {
  if(code==0)return L"正常退出";
  if(code==0xc0000135)return L"缺少运行所需 DLL；请保留引擎原有 DLL 目录并核对依赖";
  if(code==0xc000007b)return L"可执行文件或 DLL 格式/位数不匹配";
  if(code==0xc0000142)return L"DLL 初始化失败";
  if(code==0xc000013a)return L"已请求强制终止";
  return L"引擎异常退出，请检查 stdout/stderr 和引擎依赖";
}
std::wstring executable_path(){std::wstring path(32768,L'\0');const auto n=GetModuleFileNameW(nullptr,path.data(),static_cast<DWORD>(path.size()));if(!n||n>=path.size())throw std::runtime_error("Cannot locate executable");path.resize(n);return path;}
bool check_engine(const std::wstring& path,std::wstring& description) {
  const auto attributes=GetFileAttributesW(path.c_str());
  if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_DIRECTORY)){description=L"请选择存在的 video-compare.exe 文件。";return false;}
  DWORD type{};if(!GetBinaryTypeW(path.c_str(),&type)||(type!=SCS_64BIT_BINARY&&type!=SCS_32BIT_BINARY)){description=L"所选文件不是可运行的 Windows 32/64 位 EXE："+system_error(GetLastError());return false;}
  description=type==SCS_64BIT_BINARY?L"Windows 64 位可执行文件":L"Windows 32 位可执行文件";return true;
}
std::string utf8(const std::wstring& text){if(text.empty())return {};const auto n=WideCharToMultiByte(CP_UTF8,0,text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,nullptr);std::string out(n,'\0');WideCharToMultiByte(CP_UTF8,0,text.data(),static_cast<int>(text.size()),out.data(),n,nullptr,nullptr);return out;}
std::wstring Utf8Decoder::feed(const std::string& bytes,bool final) {
  pending_+=bytes;std::wstring out;std::size_t i=0;
  while(i<pending_.size()) {
    const auto c=static_cast<unsigned char>(pending_[i]);int length=c<0x80?1:(c>=0xc2&&c<=0xdf)?2:(c>=0xe0&&c<=0xef)?3:(c>=0xf0&&c<=0xf4)?4:0;
    if(!length){out+=L'\xfffd';++i;continue;}
    if(i+length>pending_.size()){if(!final)break;out+=L'\xfffd';++i;continue;}
    unsigned value=length==1?c:c&((1u<<(7-length))-1);bool valid=true;
    for(int j=1;j<length;++j){auto b=static_cast<unsigned char>(pending_[i+j]);if((b&0xc0)!=0x80){valid=false;break;}value=(value<<6)|(b&0x3f);}
    if(!valid||(length==2&&value<0x80)||(length==3&&value<0x800)||(length==4&&value<0x10000)||value>0x10ffff||(value>=0xd800&&value<=0xdfff)){out+=L'\xfffd';++i;continue;}
    if(value==0)out+=L'\u2400';else if(value<=0xffff)out+=static_cast<wchar_t>(value);else {value-=0x10000;out+=static_cast<wchar_t>(0xd800+(value>>10));out+=static_cast<wchar_t>(0xdc00+(value&1023));}
    i+=length;
  }
  pending_.erase(0,i);return out;
}
void Run::close_handles(){if(worker_)CloseHandle(worker_);if(stop_)CloseHandle(stop_);if(kill_)CloseHandle(kill_);worker_=stop_=kill_=nullptr;}
Run::~Run(){close_handles();}
bool Run::active() const{return worker_&&WaitForSingleObject(worker_,0)==WAIT_TIMEOUT;}
bool Run::start(const LaunchPlan& plan,const std::wstring& root,DWORD timeout,std::wstring& error) {
  error.clear();if(active()){error=L"已有任务运行，请先停止或等待结束。";return false;}
  std::wstring info;if(!check_engine(plan.executable,info)){error=info;return false;}
  std::error_code ec;if(!std::filesystem::is_directory(plan.working_directory,ec)){error=L"工作目录不存在或无法访问，请重新选择。";return false;}
  const auto helper=(std::filesystem::path(executable_path()).parent_path()/L"video-compare-runner.exe").wstring();
  if(!check_engine(helper,info)){error=L"缺少同目录的 video-compare-runner.exe，请重新构建并安装 GUI。";return false;}
  try {
    close_handles();last_={};GUID guid{};if(FAILED(CoCreateGuid(&guid)))throw std::runtime_error("Cannot create session ID");wchar_t id[40]{};StringFromGUID2(guid,id,40);
    folder_=(std::filesystem::path(root)/id).wstring();std::filesystem::create_directories(folder_);
    stop_=CreateEventW(nullptr,TRUE,FALSE,event_name(folder_,L"-stop").c_str());kill_=CreateEventW(nullptr,TRUE,FALSE,event_name(folder_,L"-kill").c_str());
    if(!stop_||!kill_)throw std::runtime_error("Cannot create session events");
    std::string data;number(data,0x35474356);number(data,timeout);string(data,plan.executable);string(data,plan.working_directory);number(data,static_cast<DWORD>(plan.arguments.size()));for(const auto& arg:plan.arguments)string(data,arg);
    write_file(join(folder_,L"request.bin"),data);
    write_file(join(folder_,L"session.txt"),utf8(L"开始请求："+stamp()+L"\r\n"+plan.preview+L"\r\n捕获按收到顺序显示，不保证 stdout/stderr 原始交错次序。无效 UTF-8 用替代字符显示，原始字节保留于轮转日志。\r\n"));
    save_status(folder_,{Phase::Starting,0,0,L"准备启动"});
    const auto command=make_windows_command_line({helper,L"--request",join(folder_,L"request.bin")});std::vector<wchar_t> mutable_command(command.begin(),command.end());mutable_command.push_back(0);
    STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION pi{};
    if(!CreateProcessW(helper.c_str(),mutable_command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,folder_.c_str(),&startup,&pi)){error=L"无法启动运行助手："+system_error(GetLastError());close_handles();return false;}
    worker_=pi.hProcess;CloseHandle(pi.hThread);return true;
  }catch(...){close_handles();error=L"无法创建运行记录或进程资源，请检查日志目录权限与可用空间。";return false;}
}
Snapshot Run::poll() {
  if(folder_.empty())return last_;
  try {const auto bytes=read_file(join(folder_,L"state.bin"),65536);if(!bytes.empty()){Reader reader{bytes};last_.phase=static_cast<Phase>(reader.number());last_.pid=reader.number();last_.exit_code=reader.number();last_.message=reader.string();}}
  catch(...){/* A missing/incomplete snapshot must not stop pipe draining. */}
  Utf8Decoder decoder;last_.tail=decoder.feed(read_file(join(folder_,L"tail.txt"),524288),true);last_.active=active();
  if(worker_&&!last_.active&&last_.phase!=Phase::Exited&&last_.phase!=Phase::Failed){DWORD code{};GetExitCodeProcess(worker_,&code);last_.phase=Phase::Failed;last_.exit_code=code;last_.message=L"运行助手未正常完成，请检查日志目录、磁盘空间与权限。";}
  return last_;
}
void Run::request_stop(){if(stop_)SetEvent(stop_);}
void Run::force_stop(){if(kill_)SetEvent(kill_);}

int run_request(const std::wstring& request_file) {
  SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
  const auto folder=std::filesystem::path(request_file).parent_path().wstring();Status status;Handle engine;
  try {
    const auto bytes=read_file(request_file,4*1024*1024);Reader reader{bytes};if(reader.number()!=0x35474356)throw std::runtime_error("Invalid request magic");
    const auto timeout=reader.number();const auto exe=reader.string(),work=reader.string();const auto count=reader.number();if(!count||count>16384)throw std::runtime_error("Invalid arguments");
    std::vector<std::wstring> args;for(DWORD i=0;i<count;++i)args.push_back(reader.string());
    if(reader.offset!=bytes.size()||args[0]!=exe||!std::filesystem::path(exe).is_absolute()||!std::filesystem::path(work).is_absolute())throw std::runtime_error("Invalid request fields");
    const auto command=make_windows_command_line(args);if(command.size()>=32767)throw std::runtime_error("Command too long");
    Handle stop(CreateEventW(nullptr,TRUE,FALSE,event_name(folder,L"-stop").c_str())),kill(CreateEventW(nullptr,TRUE,FALSE,event_name(folder,L"-kill").c_str()));
    if(!stop.value||!kill.value)throw std::runtime_error("Cannot open control events");
    Pipe out,err;RawLog out_log(join(folder,L"stdout.raw")),err_log(join(folder,L"stderr.raw"));Tail tail;Utf8Decoder out_decoder,err_decoder;
    SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};Handle input(CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
    if(input.value==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot open stdin");
    HANDLE inherited[]={out.write.value,err.write.value,input.value};Attributes attributes(inherited,3);
    STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdOutput=out.write.value;startup.StartupInfo.hStdError=err.write.value;startup.StartupInfo.hStdInput=input.value;startup.lpAttributeList=attributes.list;
    std::vector<wchar_t> mutable_command(command.begin(),command.end());mutable_command.push_back(0);PROCESS_INFORMATION pi{};
    if(!CreateProcessW(exe.c_str(),mutable_command.data(),nullptr,nullptr,TRUE,EXTENDED_STARTUPINFO_PRESENT|CREATE_NO_WINDOW,nullptr,work.c_str(),&startup.StartupInfo,&pi)) {
      status={Phase::Failed,0,GetLastError(),L"创建比较进程失败："+system_error(GetLastError())};save_status(folder,status);tail.append(L"启动器",status.message);write_file(join(folder,L"tail.txt"),utf8(tail.value),true);return 1;
    }
    engine.reset(pi.hProcess);CloseHandle(pi.hThread);out.write.reset();err.write.reset();input.reset();
    status={Phase::Running,pi.dwProcessId,0,L"进程已启动（尚不代表视频加载成功）"};save_status(folder,status);tail.append(L"启动器",stamp()+L" PID="+std::to_wstring(status.pid)+L" "+status.message);
    const auto start=GetTickCount64();ULONGLONG stopping{},exited{},saved{};bool killed=false,warned=false,timeout_fired=false;
    while(true) {
      const auto now=GetTickCount64();
      drain(out,out_log,out_decoder,tail,L"stdout");drain(err,err_log,err_decoder,tail,L"stderr");
      if((out_log.failed||err_log.failed)&&!warned){tail.append(L"启动器",L"原始日志写入失败，继续排空管道；请检查磁盘空间与权限。");warned=true;}
      const bool alive=WaitForSingleObject(engine.value,0)==WAIT_TIMEOUT;
      if(alive&&!stopping&&WaitForSingleObject(stop.value,0)==WAIT_OBJECT_0){stopping=now;EnumWindows(close_window,status.pid);status.phase=Phase::Stopping;status.message=L"已请求正常关闭，等待引擎退出";save_status(folder,status);tail.append(L"启动器",status.message);}
      if(alive&&stopping&&now-stopping>=5000&&status.phase!=Phase::StopTimedOut){status.phase=Phase::StopTimedOut;status.message=L"正常关闭超时，可选择强制结束或继续等待";save_status(folder,status);tail.append(L"启动器",status.message);}
      if(alive&&!killed&&(WaitForSingleObject(kill.value,0)==WAIT_OBJECT_0||(timeout&&now-start>=timeout))) {
        timeout_fired=timeout&&now-start>=timeout;
        if(TerminateProcess(engine.value,0xc000013a)){killed=true;tail.append(L"启动器",timeout_fired?L"引擎检查超时，已结束本次检查进程。":L"已强制终止本启动器创建的引擎进程。");}
        else {tail.append(L"启动器",L"结束进程失败："+system_error(GetLastError()));ResetEvent(kill.value);}
      }
      if(!alive&&!exited)exited=now;
      if(exited&&(!out.read.value&&!err.read.value||now-exited>=1500)) {
        if(out.read.value||err.read.value)tail.append(L"启动器",L"引擎已退出，后代进程仍持有输出句柄；已停止等待并释放管道。");
        tail.append(L"stdout",out_decoder.feed({},true));tail.append(L"stderr",err_decoder.feed({},true));
        GetExitCodeProcess(engine.value,&status.exit_code);status.phase=Phase::Exited;
        status.message=(timeout_fired?L"检查超时；":L"")+exit_description(status.exit_code)+L"；退出码="+std::to_wstring(status.exit_code);
        tail.append(L"启动器",stamp()+L" "+status.message);write_file(join(folder,L"tail.txt"),utf8(tail.value),true);save_status(folder,status);return 0;
      }
      if(now-saved>=200){write_file(join(folder,L"tail.txt"),utf8(tail.value),true);saved=now;}
      Sleep(10);
    }
  }catch(...) {
    // A failed helper must not strand a child whose redirected pipes cannot drain.
    if(engine.value&&WaitForSingleObject(engine.value,0)==WAIT_TIMEOUT){TerminateProcess(engine.value,0xc000013a);WaitForSingleObject(engine.value,2000);}
    try{status.phase=Phase::Failed;status.message=L"运行助手发生错误；请检查请求文件、日志目录权限和磁盘空间。";save_status(folder,status);}catch(...){}
    return 2;
  }
}
}
