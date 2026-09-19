#include "core/storage.h"
#include "core/catalog.h"
#include <filesystem>
#include <iostream>
#include <functional>
using namespace launcher;
namespace st=launcher::storage;
int checks=0;
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);++checks;std::cout<<"PASS: "<<text<<'\n';}
void rejects(const std::function<void()>& f,const char* text){bool failed=false;try{f();}catch(...){failed=true;}check(failed,text);}
enum class Mode{Flag,Value,QueryFlag,QueryValue};
struct Test{const char* id;const char* flag;Mode mode;Scope scope;const wchar_t* value;};
const Test cases[]{
#include "cli_cases.inc"
};
int wmain(int argc,wchar_t** argv){std::cout<<std::unitbuf;try{
 if(argc!=2)return 2;auto root=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(root);
 auto path=(root/L"会话 测试.vcgui").wstring(),opt=(root/L"参数 # 测试.opt").wstring();
 Session base=make_session(root.wstring());base.working_directory=root.wstring();base.engine_path=(root/L"engine.exe").wstring();base.left={InputKind::File,(root/L"左 侧.mp4").wstring()};add_right(base,{InputKind::Address,L"https://example.test/right?q=1&x=2"});
 for(auto& c:cases){if(c.scope==Scope::Query||c.scope==Scope::Config)continue;auto s=base;std::wstring error;check(set_option(s,c.id,c.mode==Mode::Flag?OptionValue(true):OptionValue(std::wstring(c.value)),error),c.id);st::save_session(path,s);auto loaded=st::load_session(path);check(build_comparison(s).plan->arguments==build_comparison(loaded).plan->arguments,"CLI session roundtrip");}
 auto s=base;s.configuration.load_automatic_file=true;s.configuration.files={opt,L"C:\\有 空格\\other.opt"};s.global["fullscreen"]=false;s.common["filters"]=L"scale=640:-2";s.left_options["color-range"]=L"";s.right_defaults["decoder"]=L"h264";
 add_right(s,{InputKind::ImageSequence,L"frames\\%04d.png"});add_right(s,{InputKind::Script,L"script.avs"});add_right(s,{InputKind::Reference,L"__"});s.right_inputs[1].enabled=false;
 for(auto& d:override_catalog())s.right_inputs[0].overrides[d.key]={OverrideMode::Clear,L""};s.right_inputs[1].overrides["filters"]={OverrideMode::Append,L"hflip"};s.right_inputs[2].overrides["filters"]={OverrideMode::Inherit,L"retained draft"};s.right_inputs[3].overrides["filters"]={OverrideMode::Replace,L""};
 move_right(s,s.right_inputs[3].id,0);st::save_session(path,s);auto loaded=st::load_session(path);auto bytes=st::read_bytes(path);st::save_session(path,loaded);check(bytes==st::read_bytes(path),"All fields, ordering, inactive values and override states roundtrip byte-for-byte");
 check(loaded.right_inputs[2].enabled==false&&loaded.configuration.files==s.configuration.files,"Disabled inputs and ordered configuration preserved");check(loaded.right_inputs[2].source.text==(root/L"frames/%04d.png").lexically_normal().wstring(),"Relative local paths anchored to working directory");
 st::write_bytes(path,bytes.substr(0,bytes.size()-3));rejects([&]{st::load_session(path);},"Truncated session rejected");auto corrupt=bytes;corrupt[30]^=1;st::write_bytes(path,corrupt);rejects([&]{st::load_session(path);},"Damaged payload rejected by checksum");st::write_bytes(path,"VCGUI000old");rejects([&]{st::load_session(path);},"Unknown old format rejected");s.schema_version=2;rejects([&]{st::save_session(path,s);},"Future schema rejected");s=base;s.right_inputs[0].id=0;st::save_session(path,s);rejects([&]{st::load_session(path);},"Invalid stable IDs rejected");s=base;s.right_inputs[0].source.kind=static_cast<InputKind>(99);st::save_session(path,s);rejects([&]{st::load_session(path);},"Invalid enum rejected");
 check(st::tokenize(L"--verbose # rest\n--fullscreen")==std::vector<std::wstring>{L"--verbose"},"Hash consumes remainder of file, not one line");
 check(st::tokenize(L"'' \"\" x")==std::vector<std::wstring>{L"x"},"Engine discards standalone empty quoted tokens");
 check(st::tokenize(L"'C:\\a b' \"x\\\"y\" z\\ w")==std::vector<std::wstring>{L"C:\\a b",L"x\"y",L"z w"},"Single/double quotes and escapes match engine");
 rejects([&]{st::tokenize(L"\"bad");},"Unterminated quote rejected");rejects([&]{st::serialize_options({L"--filters",L""});},"Empty argument export blocked without changing meaning");
 std::vector<std::wstring> tokens{L"--filters",L"drawtext=text='a#b':fontfile=C:\\font.ttf",L"a\nb",L"尾\\"};check(st::tokenize(st::serialize_options(tokens))==tokens,"Opt serializer roundtrips quotes, hash, newline and trailing slash");
 st::write_bytes(opt,st::encode(L"--mode=hstack -a off --frame-buffer-size 80 --frame-buffer-size 90 --color-range limited:full"));auto imported=st::import_options(opt,base);check(imported.editable&&std::get<std::wstring>(imported.session.global.at("frame-buffer-size"))==L"90","Import aliases, equals values, repeated last wins");check(imported.session.left_options.at("color-range")==L"limited"&&imported.session.right_defaults.at("color-range")==L"full","Paired scope import");
 s=base;s.right_inputs[0].overrides["filters"]={OverrideMode::Clear,L""};s.right_inputs[0].overrides["decoder"]={OverrideMode::Replace,L"h264"};st::export_options(opt,s);imported=st::import_options(opt,base);check(imported.editable&&build_comparison(imported.session).plan->arguments==build_comparison(s).plan->arguments,"Full .opt export/import preserves active argv and right-video specs");
 for(auto text:{L"--new-engine-option yes",L"--options-file 'nested.opt'",L"--help",L"--filters",L"only-one.mp4",L"\xfeff--verbose"}){st::write_bytes(opt,st::encode(text));auto r=st::import_options(opt,base);check(!r.editable&&r.session.right_inputs[0].source.text==base.right_inputs[0].source.text,"Unrepresentable config retains session and offers compatibility reference");}
 st::write_bytes(opt,"--fullscreen --frame-buffer-size 70");s=base;s.configuration.files={opt};s.global["fullscreen"]=false;s.global["frame-buffer-size"]=L"90";auto report=st::inspect_sources(s);check(report.find(L"--fullscreen")!=std::wstring::npos&&report.find(L"90")!=std::wstring::npos,"Effective config keeps inherited true flag and later CLI scalar");rejects([&]{st::export_options(opt,s);},"External references cannot silently become nested export");
 auto prepared=st::prepare_launch(*build_comparison(s).plan,(root/L"cache").wstring());check(prepared.arguments[2]!=opt&&st::read_bytes(prepared.arguments[2])==st::read_bytes(opt),"Unicode .opt path staged byte-for-byte");
 s=base;s.common["filters"]=std::wstring(34000,L'x');check(!build_comparison(s).ok(),"Default core API still rejects oversized command");auto full=build_comparison(s,true);prepared=st::prepare_launch(*full.plan,(root/L"cache").wstring());check(prepared.command_line.size()<32767,"Long command uses short .opt invocation");auto params=st::tokenize(st::decode(st::read_bytes(prepared.arguments.back())));auto expected=full.plan->arguments;expected.erase(expected.begin());expected.erase(std::find(expected.begin(),expected.end(),L"--no-auto-options-file"));expected.erase(std::find(expected.begin(),expected.end(),L"--"));check(params==expected,"Long .opt preserves values and excludes file-level stop delimiter");
 s.common["decoder"]=L"";rejects([&]{st::prepare_launch(*build_comparison(s,true).plan,(root/L"cache").wstring());},"Long command with empty argument fails safely");
 st::write_bytes(opt,"--auto-loop-mode invalid");check(!st::import_options(opt,base).editable,"Invalid known value is not silently replaced by a GUI default");
 st::write_bytes(opt,"--color-range limited:full");s=base;s.configuration.files={opt};s.common["color-range"]=L"limited";report=st::inspect_sources(s);auto section=report.substr(report.find(L"已解析来源合并后的参数预览"));check(section.find(L"limited:full")==std::wstring::npos,"Later paired CLI option replaces both earlier file sides");
 s=base;s.right_inputs[0].source={InputKind::Address,L"-dash-input"};rejects([&]{st::export_options(opt,s);},"Dash input export refused when file-level delimiter would corrupt bootstrap parsing");
 std::wstring help;for(auto& d:option_catalog())help+=d.flag+L" ";check(st::compatibility(help).find(L"未发现源码基线选项：无")!=std::wstring::npos,"Help capability match");check(st::compatibility(L"--version --help --future").find(L"--future")!=std::wstring::npos,"Help reports unknown and missing capabilities");
 std::cout<<"RESULT passed="<<checks<<" failed=0\n";
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}return 0;}
