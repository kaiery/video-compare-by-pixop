#include "storage.h"
#include "catalog.h"
#include <windows.h>
#include <objbase.h>
#include <filesystem>
#include <set>
#include <regex>
#include <stdexcept>
#include <algorithm>

namespace launcher::storage {
namespace {
constexpr size_t limit=16*1024*1024;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Handle {HANDLE h;~Handle(){if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);}};
void num(std::string& b,uint64_t n){for(int i=0;i<8;++i)b+=static_cast<char>((n>>(8*i))&255);}
uint64_t hash(const std::string& b){uint64_t h=14695981039346656037ull;for(unsigned char c:b){h^=c;h*=1099511628211ull;}return h;}
void str(std::string& b,const std::wstring& s){auto v=encode(s);num(b,v.size());b+=v;}
void key(std::string& b,const std::string& s){str(b,{s.begin(),s.end()});}
struct Reader {
 const std::string& b;size_t p{};
 uint64_t n(){require(p+8<=b.size(),"Truncated session");uint64_t v=0;for(int i=0;i<8;++i)v|=uint64_t(static_cast<unsigned char>(b[p++]))<<(8*i);return v;}
 size_t count(){auto v=n();require(v<=100000,"Excessive record count");return static_cast<size_t>(v);}
 bool boolean(){auto v=n();require(v<=1,"Invalid boolean");return v!=0;}
 std::wstring s(){auto z=n();require(z<=limit&&z<=b.size()-p,"Invalid text length");auto v=decode(b.substr(p,static_cast<size_t>(z)));p+=static_cast<size_t>(z);require(v.find(L'\0')==std::wstring::npos,"NUL in session text");return v;}
 std::string k(){auto v=s();require(std::all_of(v.begin(),v.end(),[](wchar_t c){return c>0&&c<128;}),"Invalid key");return encode(v);}
};
void source(std::string& b,const InputSource& s){num(b,static_cast<unsigned>(s.kind));str(b,s.text);}
InputSource source(Reader& r){auto k=r.n();require(k<=4,"Invalid input kind");return {static_cast<InputKind>(k),r.s()};}
void inputs(std::string& b,const InputOptions& map){num(b,map.size());for(const auto& e:map){key(b,e.first);str(b,e.second);}}
InputOptions inputs(Reader& r,Scope scope){InputOptions map;auto n=r.count();while(n--){auto k=r.k();bool known=false;for(const auto& d:option_catalog())if(d.field==k&&(d.scope==scope||d.scope==Scope::Paired))known=true;require(known,"Unknown scoped field");auto v=r.s();require(map.emplace(k,v).second,"Duplicate field");}return map;}
std::wstring absolute(const std::wstring& text,const std::filesystem::path& base){if(text.empty())return text;auto p=std::filesystem::path(text);return (p.is_absolute()?p:base/p).lexically_normal().wstring();}
Session normalized(Session s){auto cwd=std::filesystem::current_path();s.working_directory=absolute(s.working_directory,cwd);auto base=s.working_directory.empty()?cwd:std::filesystem::path(s.working_directory);s.engine_path=absolute(s.engine_path,base);auto fix=[&](InputSource& i){if(i.kind!=InputKind::Reference&&i.kind!=InputKind::Address)i.text=absolute(i.text,base);};fix(s.left);for(auto& i:s.right_inputs)fix(i.source);for(auto& f:s.configuration.files)f=absolute(f,base);return s;}
std::string narrow(const std::wstring& s){require(std::all_of(s.begin(),s.end(),[](wchar_t c){return c>0&&c<128;}),"Non-ASCII option name");return encode(s);}
InputSource imported_source(const std::wstring& text){auto colon=text.find(L':');auto kind=text==L"__"?InputKind::Reference:colon!=std::wstring::npos&&colon!=1?InputKind::Address:text.find(L'%')!=std::wstring::npos?InputKind::ImageSequence:InputKind::File;auto ext=std::filesystem::path(text).extension().wstring();if(kind==InputKind::File&&(ext==L".avs"||ext==L".vpy"))kind=InputKind::Script;return {kind,text};}
Session apply_tokens(const std::vector<std::wstring>& tokens,Session s){
 std::vector<std::wstring> paths;bool positional=false;std::wstring error;
 for(size_t i=0;i<tokens.size();++i){auto t=tokens[i];if(t==L"--"&&!positional){positional=true;continue;}
  if(positional||t.empty()||t[0]!=L'-'){paths.push_back(t);continue;}
  auto eq=t.find(L'=');auto name=t.substr(0,eq);auto* d=find_option(narrow(name));require(d&&d->scope!=Scope::Query,"Unknown/query option requires compatibility reference");
  require(d->key!="options-file","Nested options-file requires compatibility reference");
  OptionValue value=true;if(d->takes_value()){if(eq!=std::wstring::npos)value=t.substr(eq+1);else{require(i+1<tokens.size(),"Missing option value");value=tokens[++i];}}else require(eq==std::wstring::npos,"Unexpected flag value");
  require(set_option(s,d->key,value,error),"Cannot represent option scope");
 }
 if(!paths.empty()){
  require(paths.size()>=2,"Incomplete positional inputs require compatibility reference");s.left=imported_source(paths[0]);s.right_inputs.clear();
  for(size_t i=1;i<paths.size();++i){auto sep=paths[i].find(L"::");auto id=add_right(s,imported_source(paths[i].substr(0,sep)));(void)id;auto& right=s.right_inputs.back();
   while(sep!=std::wstring::npos){auto begin=sep+2;sep=paths[i].find(L"::",begin);auto field=paths[i].substr(begin,sep==std::wstring::npos?sep:sep-begin);auto eq=field.find(L'=');require(eq!=std::wstring::npos,"Invalid right-video syntax");auto k=narrow(field.substr(0,eq));auto* d=find_override(k);require(d&&d->key==k,"Unknown right-video field");auto v=field.substr(eq+1);right.overrides[k]={v.empty()?OverrideMode::Clear:OverrideMode::Replace,v};}
  }
 }
 return s;
}
}
std::string encode(const std::wstring& t){if(t.empty())return {};int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,t.data(),static_cast<int>(t.size()),nullptr,0,nullptr,nullptr);require(n>0,"Invalid Unicode");std::string b(n,0);WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,t.data(),static_cast<int>(t.size()),b.data(),n,nullptr,nullptr);return b;}
std::wstring decode(const std::string& b){if(b.empty())return {};int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,b.data(),static_cast<int>(b.size()),nullptr,0);require(n>0,"File is not valid UTF-8");std::wstring t(n,0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,b.data(),static_cast<int>(b.size()),t.data(),n);return t;}
std::string read_bytes(const std::wstring& p){Handle f{CreateFileW(p.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)};require(f.h!=INVALID_HANDLE_VALUE,"Cannot read file");LARGE_INTEGER z{};require(GetFileSizeEx(f.h,&z)&&z.QuadPart>=0&&z.QuadPart<=limit,"File exceeds 16 MiB limit");std::string b(static_cast<size_t>(z.QuadPart),0);DWORD n{};require(b.empty()||(ReadFile(f.h,b.data(),static_cast<DWORD>(b.size()),&n,nullptr)&&n==b.size()),"Incomplete file read");return b;}
void write_bytes(const std::wstring& p,const std::string& b){require(b.size()<=limit,"File exceeds 16 MiB limit");GUID g{};require(SUCCEEDED(CoCreateGuid(&g)),"Cannot create temporary file ID");wchar_t id[40]{};StringFromGUID2(g,id,40);auto temp=p+id+L".tmp";try{{Handle f{CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr)};require(f.h!=INVALID_HANDLE_VALUE,"Cannot create file");DWORD n{};require(WriteFile(f.h,b.data(),static_cast<DWORD>(b.size()),&n,nullptr)&&n==b.size()&&FlushFileBuffers(f.h),"Cannot save complete file");}require(MoveFileExW(temp.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0,"Cannot replace destination");}catch(...){DeleteFileW(temp.c_str());throw;}}
void save_session(const std::wstring& path,const Session& original){
 auto s=normalized(original);require(s.schema_version==1,"Unsupported session version");std::string b="VCGUI001";num(b,s.schema_version);str(b,s.engine_path);str(b,s.working_directory);num(b,s.next_input_id);
 num(b,s.global.size());for(const auto& e:s.global){key(b,e.first);num(b,std::holds_alternative<std::wstring>(e.second));if(auto v=std::get_if<bool>(&e.second))num(b,*v);else str(b,std::get<std::wstring>(e.second));}
 inputs(b,s.common);source(b,s.left);inputs(b,s.left_options);inputs(b,s.right_defaults);num(b,s.configuration.load_automatic_file);num(b,s.configuration.files.size());for(auto& f:s.configuration.files)str(b,f);
 num(b,s.right_inputs.size());for(const auto& i:s.right_inputs){num(b,i.id);num(b,i.enabled);source(b,i.source);num(b,i.overrides.size());for(const auto& e:i.overrides){key(b,e.first);num(b,static_cast<unsigned>(e.second.mode));str(b,e.second.value);}}
 num(b,hash(b));write_bytes(path,b);
}
Session load_session(const std::wstring& path){
 auto b=read_bytes(path);require(b.size()>=24&&b.substr(0,8)=="VCGUI001","Unknown/old session format");Reader trailer{b,b.size()-8};require(trailer.n()==hash(b.substr(0,b.size()-8)),"Session checksum mismatch");b.resize(b.size()-8);Reader r{b,8};Session s;auto version=r.n();require(version==1,"Unsupported schema version");s.engine_path=r.s();s.working_directory=r.s();s.next_input_id=r.n();require(s.next_input_id>0,"Invalid next ID");auto n=r.count();while(n--){auto k=r.k();auto* d=find_option(k);require(d&&d->key==k&&d->scope==Scope::Global,"Unknown global option");bool text=r.boolean();require(text==d->takes_value(),"Wrong option type");OptionValue v=text?OptionValue(r.s()):OptionValue(r.boolean());require(s.global.emplace(k,v).second,"Duplicate global option");}
 s.common=inputs(r,Scope::Common);s.left=source(r);s.left_options=inputs(r,Scope::Left);s.right_defaults=inputs(r,Scope::Right);s.configuration.load_automatic_file=r.boolean();n=r.count();while(n--)s.configuration.files.push_back(r.s());
 n=r.count();std::set<uint64_t> ids;while(n--){RightInput i;i.id=r.n();require(i.id>0&&i.id<s.next_input_id&&ids.insert(i.id).second,"Invalid/duplicate input ID");i.enabled=r.boolean();i.source=source(r);auto count=r.count();while(count--){auto k=r.k();auto* d=find_override(k);require(d&&d->key==k,"Unknown override field");auto m=r.n();require(m<=3&&(m!=2||d->allow_append),"Invalid override mode");require(i.overrides.emplace(k,InputOverride{static_cast<OverrideMode>(m),r.s()}).second,"Duplicate override field");}s.right_inputs.push_back(std::move(i));}
 require(r.p==b.size(),"Unexpected session data");return s;
}
std::vector<std::wstring> tokenize(const std::wstring& text){std::vector<std::wstring> out;std::wstring t;bool single=false,dbl=false,escape=false;for(wchar_t c:text){if(escape){t+=c;escape=false;continue;}if(c==L'\\'&&!single){escape=true;continue;}if(single){if(c==L'\'')single=false;else t+=c;continue;}if(dbl){if(c==L'"')dbl=false;else t+=c;continue;}if(c==L'#')break;if(c==L' '||c==L'\t'||c==L'\r'||c==L'\n'||c==L'\v'||c==L'\f'){if(!t.empty()){out.push_back(t);t.clear();}continue;}if(c==L'\''){single=true;continue;}if(c==L'"'){dbl=true;continue;}t+=c;}require(!single&&!dbl&&!escape,"Unterminated quote or escape");if(!t.empty())out.push_back(t);return out;}
std::wstring serialize_options(const std::vector<std::wstring>& args){std::wstring text;for(const auto& a:args){require(!a.empty(),"Engine .opt syntax cannot preserve an empty argument; use a session file");text+=L'"';for(wchar_t c:a){require(c!=0,"NUL cannot be exported");if(c==L'"'||c==L'\\')text+=L'\\';text+=c;}text+=L"\"\n";}require(tokenize(text)==args,"Options export is not lossless");return text;}
Import import_options(const std::wstring& path,const Session& current){Import result{current,false,{}};auto text=decode(read_bytes(path));try{require(text.empty()||text[0]!=0xfeff,"UTF-8 BOM is not stripped by the engine");auto tokens=tokenize(text);result.session=apply_tokens(tokens,current);for(const auto& issue:build_comparison(result.session,true).diagnostics)if(issue.code=="E_VALUE"||issue.code=="E_CONFLICT"||issue.code=="E_PAIR")throw std::runtime_error("Invalid/conflicting option values cannot be imported losslessly");result.editable=true;result.report=L"已将参数导入 GUI；同名值取最后一次。文件未修改。相对路径按工作目录解释。\r\n";for(const auto& t:tokens)result.report+=quote_windows_argument(t)+L"\r\n";}catch(const std::exception& e){result.session=current;result.report=L"无法完整转换为 GUI 设置："+decode(e.what())+L"\r\n可保留原文件为兼容引用；不重写内容。GUI 后发出的同名有值选项优先，未勾选不能撤销文件中的布尔开关。\r\n原文：\r\n"+text;}return result;}
void export_options(const std::wstring& path,const Session& s){require(!s.configuration.load_automatic_file&&s.configuration.files.empty(),"External configuration must be imported first; nested .opt expansion is not equivalent");auto built=build_comparison(s,true);require(built.ok(),"Correct session validation errors before exporting");auto a=built.plan->arguments;size_t boundary=1;for(const auto& e:built.plan->emissions){const auto* d=find_option(e.id);if(d)boundary=std::max(boundary,e.argument_index+1+(d->takes_value()?1:0));}require(a.at(boundary)==L"--","Missing input boundary");for(size_t i=boundary+1;i<a.size();++i)require(a[i].empty()||a[i][0]!=L'-',"Dash-prefixed positional input cannot be exported to .opt without changing engine merge semantics");a.erase(a.begin()+boundary);write_bytes(path,encode(serialize_options({a.begin()+1,a.end()})));}
std::wstring inspect_sources(const Session& s){
 std::wstring out=L"合并顺序：工作目录自动文件 → 显式文件（按列表顺序）→ GUI 参数。\r\n有值选项通常取最后一次；布尔开关只要任一来源启用便生效。\r\n";
 std::vector<std::wstring> files;
 if(s.configuration.load_automatic_file){auto p=std::filesystem::path(s.working_directory)/L"video-compare.opt";if(std::filesystem::exists(p))files.push_back(p.wstring());else out+=L"自动文件不存在："+p.wstring()+L"\r\n";}
 for(auto& f:s.configuration.files)files.push_back(absolute(f,s.working_directory));
 Session effective=s;effective.global.clear();effective.common.clear();effective.left_options.clear();effective.right_defaults.clear();effective.configuration={};bool exact=true;
 for(auto& f:files){out+=L"\r\n来源："+f+L"\r\n";try{auto imported=import_options(f,effective);out+=imported.report;
   if(imported.editable){if(imported.session.next_input_id!=effective.next_input_id){exact=false;out+=L"文件含位置输入；引擎会与 GUI 输入拼接，不能视为单纯设置覆盖。请导入为会话后移除此引用。\r\n";}effective=std::move(imported.session);}else exact=false;
  }catch(const std::exception& e){exact=false;out+=L"读取失败："+decode(e.what())+L"\r\n";}}
 auto cli=build_comparison(s,true);
 if(!cli.ok()){out+=L"\r\nGUI 配置尚未有效，不能生成完整合并预览：\r\n";for(auto& d:cli.diagnostics)out+=d.message+L"\r\n";return out;}
 std::wstring error;
 for(const auto& e:cli.plan->emissions){auto* d=find_option(e.id);if(d&&d->scope!=Scope::Config)set_option(effective,d->key,d->takes_value()?OptionValue(cli.plan->arguments.at(e.argument_index+1)):OptionValue(true),error);}
 effective.left=s.left;effective.right_inputs=s.right_inputs;effective.next_input_id=s.next_input_id;effective.configuration={};
 auto built=build_comparison(effective,true);
 out+=exact?L"\r\n已解析来源合并后的参数预览（非运行验证）：\r\n":L"\r\n包含未解析内容，不能给出完整有效值；以下仅为已识别部分：\r\n";
 if(built.ok())out+=built.plan->preview;else for(auto& d:built.diagnostics)out+=d.message+L"\r\n";return out;
}
std::wstring compatibility(const std::wstring& help){std::set<std::wstring> flags;std::wregex re(L"--[a-zA-Z0-9][a-zA-Z0-9-]*");for(std::wsregex_iterator i(help.begin(),help.end(),re),end;i!=end;++i)flags.insert(i->str());std::wstring missing,extra;for(auto& d:option_catalog()){if(!flags.erase(d.flag))missing+=d.flag+L" ";}for(auto& f:flags)extra+=f+L" ";return L"\r\n帮助文本能力核对（仅声明，不代表解码/设备可用）：\r\n未发现源码基线选项："+(missing.empty()?L"无":missing)+L"\r\n基线以外选项："+(extra.empty()?L"无":extra)+L"\r\n未发现不等于已证明不支持；请结合引擎版本及实际运行。不会自动丢弃参数。\r\n";}
}

namespace launcher::storage {
LaunchPlan prepare_launch(LaunchPlan plan,const std::wstring& cache_root) {
 std::filesystem::path folder;
 auto cache=[&](const std::string& bytes){
  if(folder.empty()){GUID g{};require(SUCCEEDED(CoCreateGuid(&g)),"Cannot create options ID");wchar_t id[40]{};StringFromGUID2(g,id,40);folder=std::filesystem::path(cache_root)/id;std::filesystem::create_directories(folder);}
  static unsigned sequence=0;auto file=folder/(std::to_wstring(++sequence)+L".opt");
  require(std::all_of(file.native().begin(),file.native().end(),[](wchar_t c){return c<128;}),"Engine narrow .opt file API requires an ASCII cache path; move launcher to an ASCII path");
  write_bytes(file.wstring(),bytes);return file.wstring();
 };
 std::set<size_t> external,automatic;
 for(const auto& emitted:plan.emissions){if(emitted.id=="CLI-005")external.insert(emitted.argument_index);if(emitted.id=="CLI-060")automatic.insert(emitted.argument_index);}
 for(auto i:external){auto& p=plan.arguments.at(i+1);if(std::any_of(p.begin(),p.end(),[](wchar_t c){return c>127;})){auto old=p;p=cache(read_bytes(p));plan.preview+=L"\r\n参数文件字节不变暂存："+old+L" → "+p;}}
 plan.command_line=make_windows_command_line(plan.arguments);
 if(plan.command_line.size()+1>32767){
  size_t boundary=1;for(const auto& e:plan.emissions){auto* d=find_option(e.id);if(d)boundary=std::max(boundary,e.argument_index+1+(d->takes_value()?1:0));}
  require(plan.arguments.at(boundary)==L"--","Missing input boundary");
  for(size_t i=boundary+1;i<plan.arguments.size();++i)require(plan.arguments[i].empty()||plan.arguments[i][0]!=L'-',"Dash-prefixed input cannot be moved into .opt safely");
  std::vector<std::wstring> direct{plan.executable},content;
  for(size_t i=1;i<plan.arguments.size();++i){auto& a=plan.arguments[i];
   if(i==boundary)continue; // A file-level -- would also turn the trailing bootstrap CLI into inputs.
   if(external.count(i)){direct.push_back(a);direct.push_back(plan.arguments.at(++i));}
   else if(automatic.count(i))direct.push_back(a);else content.push_back(a);
  }
  auto file=cache(encode(serialize_options(content)));direct.push_back(L"--options-file");direct.push_back(file);plan.arguments=std::move(direct);plan.command_line=make_windows_command_line(plan.arguments);plan.emissions.clear();plan.preview+=L"\r\n超长参数已无损暂存："+file;
 }
 require(plan.command_line.size()+1<=32767,"Command remains too long; reduce external configuration references");
 plan.preview+=L"\r\n实际进程命令行（不是 shell 脚本）：\r\n"+plan.command_line;return plan;
}
}
