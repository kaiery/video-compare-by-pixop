// P07 explicitly-invoked real-engine integration. Compiles the production GUI
// window and handlers, then compares its launch path with direct CreateProcessW.
#include "../src/main.cpp"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <set>
#include <functional>
#include <thread>
#include <chrono>
namespace fs=std::filesystem;
struct Case {std::string id,name;Session session;std::string condition;bool failure{};std::string action;};
struct Observation {DWORD code{};bool window{},timed_out{},action_verified{true};int width{},height{},windows{};std::wstring caption,log;std::map<std::wstring,std::string> pngs,steps;};
void pump(int ms){const auto end=GetTickCount64()+ms;do{MSG m{};while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){if(m.message!=WM_QUIT){TranslateMessage(&m);DispatchMessageW(&m);}}Sleep(10);}while(GetTickCount64()<end);}
std::vector<HWND> windows(DWORD pid){std::pair<DWORD,std::vector<HWND>> data{pid,{}};EnumWindows([](HWND h,LPARAM p)->BOOL{auto& d=*reinterpret_cast<decltype(data)*>(p);DWORD id{};GetWindowThreadProcessId(h,&id);wchar_t name[128]{};GetClassNameW(h,name,128);if(id==d.first&&IsWindowVisible(h)&&std::wstring(name)==L"SDL_app")d.second.push_back(h);return TRUE;},reinterpret_cast<LPARAM>(&data));return data.second;}
DWORD input_owner{};
void post_key(HWND h,WORD key,bool up){if(up&&!IsWindow(h))return;DWORD owner{};GetWindowThreadProcessId(h,&owner);if(!input_owner||owner!=input_owner)throw std::runtime_error("Input target is not the owned engine");auto scan=MapVirtualKeyW(key,MAPVK_VK_TO_VSC);if(!PostMessageW(h,up?WM_KEYUP:WM_KEYDOWN,key,1|(scan<<16)|(up?((1LL<<30)|(1LL<<31)):0)))throw std::runtime_error("Owned-window key message failed");}
void key_event(HWND h,WORD key){post_key(h,key,false);pump(40);post_key(h,key,true);pump(80);}
// Global input can reach the editor if foreground focus changes. Never inject it.
void chord(HWND,WORD,bool=false,bool=false,bool=false){throw std::runtime_error("Manual verification required: global keyboard injection disabled; targeted modifiers unreliable");}
void drag(HWND,int,int,int,int,bool=false){throw std::runtime_error("Manual verification required: global mouse injection disabled");}
std::string bytes(const fs::path& path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
std::wstring read_log(const fs::path& path){process::Utf8Decoder d;return d.feed(bytes(path),true);}
void write_text(const fs::path& path,const std::wstring& value){storage::write_bytes(path.wstring(),storage::encode(value));}
struct NativeProcess {
 HANDLE process{};DWORD pid{};
 ~NativeProcess(){if(process){if(WaitForSingleObject(process,0)==WAIT_TIMEOUT){TerminateProcess(process,0xc000013a);WaitForSingleObject(process,3000);}CloseHandle(process);}}
 void start(const LaunchPlan& p,const fs::path& destination){
  SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};
  HANDLE out=CreateFileW((destination/L"stdout.raw").c_str(),GENERIC_WRITE,FILE_SHARE_READ,&sa,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
  HANDLE err=CreateFileW((destination/L"stderr.raw").c_str(),GENERIC_WRITE,FILE_SHARE_READ,&sa,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
  HANDLE in=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
  STARTUPINFOW si{sizeof(si)};si.dwFlags=STARTF_USESTDHANDLES;si.hStdOutput=out;si.hStdError=err;si.hStdInput=in;PROCESS_INFORMATION pi{};auto command=p.command_line;
  bool ok=CreateProcessW(p.executable.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,p.working_directory.c_str(),&si,&pi)!=FALSE;auto error=GetLastError();
  CloseHandle(out);CloseHandle(err);CloseHandle(in);if(!ok)throw std::runtime_error("CLI CreateProcess failed: "+std::to_string(error));process=pi.hProcess;pid=pi.dwProcessId;CloseHandle(pi.hThread);
 }
 bool active(){return WaitForSingleObject(process,0)==WAIT_TIMEOUT;}
 DWORD code(){DWORD c{};GetExitCodeProcess(process,&c);return c;}
};
void collect_pngs(const fs::path& cwd,const fs::path& destination,Observation& o){
 for(const auto& file:fs::directory_iterator(cwd))if(file.is_regular_file()&&file.path().extension()==L".png"){
  auto name=file.path().filename().wstring();o.pngs[name]=bytes(file.path());fs::rename(file.path(),destination/file.path().filename());
 }
}
Observation observe(DWORD pid,const fs::path& cwd,const fs::path& destination,const std::function<bool()>& active,const std::string& action){
 input_owner=pid;Observation o;auto until=GetTickCount64()+8000;std::vector<HWND> found;
 while(active()&&GetTickCount64()<until){found=windows(pid);if(!found.empty())break;pump(50);}
 if(found.empty())return o;o.window=true;pump(900);if(!active())return o;
 found=windows(pid);o.windows=static_cast<int>(found.size());HWND h=found.back(); // Main window is normally first-created, last in Z-order.
 for(auto w:found){if(text(w).find(L"|")!=std::wstring::npos){h=w;break;}}
 o.caption=text(h);RECT rect{};GetClientRect(h,&rect);o.width=rect.right;o.height=rect.bottom;
 PostMessageW(h,WM_MOUSEMOVE,0,MAKELPARAM(rect.right/2,rect.bottom/2));pump(100);
 key_event(h,'3'); // Hide timing-sensitive HUD for visual comparison.
 auto snapshot=[&](const std::wstring& label){auto folder=destination/L"steps"/label;fs::create_directories(folder);Observation shot;for(int attempt=0;attempt<3&&shot.pngs.empty();++attempt){key_event(h,'F');pump(500);collect_pngs(cwd,folder,shot);}for(auto& file:shot.pngs)if(file.first.find(L"osd")!=std::wstring::npos){o.steps[label]=file.second;return file.second;}throw std::runtime_error("No rendered screenshot for intermediate state");};
 if(action=="metrics")key_event(h,'M');
 if(action=="switch") {for(int i=0;i<12;++i){key_event(h,VK_TAB);pump(100);o.caption+=L"\n"+text(h);}}
 if(action=="RUN-001"){for(WORD k:{WORD('H'),WORD('V'),WORD('1'),WORD('2'),WORD('3')}){auto prefix=std::to_wstring(k);auto before=snapshot(prefix+L"-before");key_event(h,k);auto changed=snapshot(prefix+L"-on");key_event(h,k);auto restored=snapshot(prefix+L"-restored");o.action_verified=o.action_verified&&before!=changed&&before==restored;}o.steps.erase(L"51-on");} // HUD shows measured frame rate; validate its toggle/restore, not cross-process text bytes.
 if(action=="RUN-002"){for(WORD k:{WORD(VK_SPACE),WORD(VK_SPACE),WORD(VK_OEM_COMMA),WORD(VK_OEM_PERIOD),WORD('J'),WORD('L')})key_event(h,k);}
 if(action=="RUN-003"){for(WORD k:{WORD(VK_RIGHT),WORD(VK_LEFT),WORD(VK_UP),WORD(VK_DOWN),WORD(VK_NEXT),WORD(VK_PRIOR),WORD('A'),WORD('D')})key_event(h,k);chord(h,'A',true);chord(h,'D',true);}
 if(action=="RUN-004"){for(WORD k:{WORD('Z'),WORD('C'),WORD('E'),WORD('R'),WORD('4'),WORD('5'),WORD('6'),WORD('7'),WORD('8'),WORD('9')})key_event(h,k);PostMessageW(h,WM_MOUSEWHEEL,MAKEWPARAM(0,WHEEL_DELTA),0);}
 if(action=="RUN-005"){key_event(h,'S');for(int i=0;i<12;++i)key_event(h,VK_TAB);for(WORD k='1';k<='9';++k)chord(h,k,true,true);chord(h,'0',true,true);}
 if(action=="RUN-006"){auto before=snapshot(L"video");key_event(h,'0');auto diff=snapshot(L"difference");o.action_verified=before!=diff;for(int i=0;i<4;++i){key_event(h,'Y');snapshot(L"mode-"+std::to_wstring(i));}key_event(h,'U');auto luma=snapshot(L"luma");key_event(h,'U');snapshot(L"rgb");o.action_verified=o.action_verified&&luma!=diff;}
 if(action=="RUN-007"){for(auto k:{VK_ADD,VK_SUBTRACT}){key_event(h,static_cast<WORD>(k));chord(h,static_cast<WORD>(k),false,true);chord(h,static_cast<WORD>(k),false,false,true);}}
 if(action=="RUN-008"){key_event(h,'P');key_event(h,'M');key_event(h,'X');chord(h,'X',true);}
 if(action=="RUN-010"){key_event(h,'I');key_event(h,'T');chord(h,'M',true);chord(h,'S',true);chord(h,VK_RETURN,false,false,true);chord(h,VK_RETURN,false,false,true);}
 if(action=="RUN-009"){chord(h,'F',true);drag(h,40,40,200,160);pump(400);}
 if(action=="RUN-011"){for(WORD k:{WORD('L'),WORD('R'),WORD('B')}){chord(h,k,true);drag(h,40,40,200,160);chord(h,'X',true);key_event(h,'F');pump(350);key_event(h,VK_BACK);pump(200);}chord(h,'L',true);drag(h,40,40,200,160);chord(h,'L',true,true);chord(h,'R',true,true);chord(h,'L',false,true);chord(h,'R',false,true);chord(h,'X',true);}
 if(action=="RUN-012"){for(auto k:{VK_F1,VK_F2,VK_F3})key_event(h,static_cast<WORD>(k));pump(500);o.caption+=L"\nscopes-open="+std::to_wstring(windows(pid).size());for(auto k:{VK_F1,VK_F2,VK_F3})key_event(h,static_cast<WORD>(k));pump(300);o.caption+=L"\nscopes-closed="+std::to_wstring(windows(pid).size());}
 if(action=="RUN-013"){chord(h,'W',true,true);SetWindowPos(h,nullptr,0,0,620,440,SWP_NOMOVE|SWP_NOZORDER);pump(150);chord(h,'W',true);chord(h,'W',false,true);}
 if(action=="RUN-014"){drag(h,80,80,160,100,true);drag(h,120,100,120,100);chord(h,'X',true);}
 // SDL can discard keyboard messages while acquiring focus during startup.
 // Retry only until a screenshot exists; never interpret a sent key as evidence.
 for(int attempt=0;attempt<3;++attempt){key_event(h,'F');pump(700);bool saved=false;for(auto& f:fs::directory_iterator(cwd))if(f.path().extension()==L".png")saved=true;if(saved)break;}
 collect_pngs(cwd,destination,o);
 if(action=="RUN-015"){key_event(h,VK_ESCAPE);pump(500);o.caption+=active()?L"\nescape-exited=false":L"\nescape-exited=true";}
 return o;
}
Observation run_case(State& state,const Case& c,const fs::path& root,bool gui){
 auto destination=root/(gui?L"gui":L"cli");fs::create_directories(destination);Observation result;
 auto built=build_comparison(c.session,true);if(!built.ok())throw std::runtime_error("Invalid case plan");auto plan=storage::prepare_launch(*built.plan,(root/L"cache").wstring());
 if(gui){apply_session(state,c.session);if(!state.result.ok()||state.result.plan->arguments!=built.plan->arguments)throw std::runtime_error("GUI controls changed the intended argv");SendMessageW(state.window,WM_COMMAND,IDC_START,0);if(state.log_window)ShowWindow(state.log_window,SW_HIDE);
  if(!state.run.active())throw std::runtime_error("GUI did not start: "+storage::encode(text(state.at(IDC_FEEDBACK))));
  auto deadline=GetTickCount64()+5000;while(!state.run.poll().pid&&state.run.active()&&GetTickCount64()<deadline)pump(30);
  result=observe(state.run.poll().pid,c.session.working_directory,destination,[&]{return state.run.active();},c.action);
  state.run.request_stop();deadline=GetTickCount64()+6500;while(state.run.active()&&GetTickCount64()<deadline)pump(30);
  if(state.run.active()){result.timed_out=true;state.run.force_stop();while(state.run.active())pump(30);}
  auto final=state.run.poll();result.code=final.exit_code;result.log=read_log(fs::path(state.run.folder())/L"stdout.raw")+read_log(fs::path(state.run.folder())/L"stderr.raw");
  for(auto file:{L"session.txt",L"stdout.raw",L"stderr.raw",L"events.log"}){auto src=fs::path(state.run.folder())/file;if(fs::exists(src))fs::copy_file(src,destination/file,fs::copy_options::overwrite_existing);}
 }else{
  NativeProcess native;native.start(plan,destination);result=observe(native.pid,c.session.working_directory,destination,[&]{return native.active();},c.action);
  for(auto h:windows(native.pid))PostMessageW(h,WM_CLOSE,0,0);auto deadline=GetTickCount64()+6500;while(native.active()&&GetTickCount64()<deadline)pump(30);
  if(native.active()){result.timed_out=true;TerminateProcess(native.process,0xc000013a);WaitForSingleObject(native.process,3000);}result.code=native.code();result.log=read_log(destination/L"stdout.raw")+read_log(destination/L"stderr.raw");write_text(destination/L"session.txt",plan.preview);
 }
 write_text(destination/L"observation.txt",L"exit="+std::to_wstring(result.code)+L" window="+std::to_wstring(result.window)+L" size="+std::to_wstring(result.width)+L"x"+std::to_wstring(result.height)+L" SDL windows="+std::to_wstring(result.windows)+L" action_verified="+std::to_wstring(result.action_verified)+L" captured_states="+std::to_wstring(result.steps.size())+L"\n"+result.caption+L"\n"+result.log);
 return result;
}
void set(Session& s,const std::string& key,const std::wstring& value){std::wstring error;if(!set_option(s,key,value,error))throw std::runtime_error("Invalid option assignment");}
void flag(Session& s,const std::string& key,bool value=true){std::wstring error;if(!set_option(s,key,value,error))throw std::runtime_error("Invalid flag");}
std::vector<Case> make_cases(const fs::path& engine,const fs::path& assets){
 Session base=make_session(assets.parent_path().wstring());base.engine_path=engine.wstring();base.left={InputKind::File,(assets/L"left.mp4").wstring()};add_right(base,{InputKind::File,(assets/L"right.mp4").wstring()});
 flag(base,"verbose");set(base,"display-mode",L"split");set(base,"auto-loop-mode",L"on");set(base,"frame-buffer-size",L"1");set(base,"window-size",L"480x320");
 std::vector<Case> cases;auto add=[&](std::string id,std::string name,const std::function<void(Session&)>& change,std::string conditional="",bool failure=false,std::string action=""){auto s=base;change(s);if(conditional=="CUDA-device"&&fs::exists(assets/L"cuda-left.mp4")){s.left.text=(assets/L"cuda-left.mp4").wstring();for(auto& r:s.right_inputs)r.source.text=(assets/L"cuda-right.mp4").wstring();}cases.push_back({id,name,std::move(s),conditional,failure,action});};
 add("GUI-012","baseline",[](auto&){});
 for(auto entry:{std::pair<const char*,const char*>{"CLI-004","verbose"},{"CLI-006","fullscreen"},{"CLI-007","high-dpi"},{"CLI-010","10-bpc"},{"CLI-011","fast-alignment"},{"CLI-012","bilinear-texture"},{"CLI-013","subtraction-mode"},{"CLI-061","disable-auto-filters"}}){add(entry.first,"on",[&](auto& s){flag(s,entry.second);},std::string(entry.first)=="CLI-010"?"display-output-depth":"");add(entry.first,"off",[&](auto& s){flag(s,entry.second,false);});}
 for(auto value:{L"auto",L"scp",L"sarasa"})add("CLI-008",storage::encode(value),[&](auto& s){set(s,"font",value);});
 add("CLI-008","missing",[&](auto& s){set(s,"font",(assets/L"missing.ttf").wstring());},"",true);
 for(auto value:{L"1",L"1.25"})add("CLI-009",storage::encode(value),[&](auto& s){set(s,"ui-scale",value);});
 for(auto value:{L"0",L"1"})add("CLI-014",storage::encode(value),[&](auto& s){set(s,"display-number",value);},"display-availability");
 add("CLI-014","invalid-device",[](auto& s){set(s,"display-number",L"99");},"",true);
 for(auto value:{L"split",L"hstack",L"vstack"}){add("CLI-015",storage::encode(value),[&](auto& s){set(s,"display-mode",value);});add("CLI-013",std::string("difference-")+storage::encode(value),[&](auto& s){set(s,"display-mode",value);s.right_inputs[0].source=s.left;flag(s,"subtraction-mode");});}
 for(auto value:{L"1280x720",L"1280x",L"x720"})add("CLI-016",storage::encode(value),[&](auto& s){set(s,"window-size",value);});
 add("CLI-017","fit",[](auto& s){unset_option(s,"window-size");flag(s,"window-fit-display");});
 for(auto value:{L"off",L"window",L"content"})add("CLI-018",storage::encode(value),[&](auto& s){set(s,"aspect-lock",value);});
 for(auto value:{L"stretch",L"original",L"dynamic",L"16:9",L"4:3",L"1:1",L"16x9",L"4x3",L"1x1"})add("CLI-019",storage::encode(value),[&](auto& s){set(s,"aspect-view-mode",value);});
 for(auto value:{L"off",L"on",L"pp"})add("CLI-020",storage::encode(value),[&](auto& s){set(s,"auto-loop-mode",value);set(s,"frame-buffer-size",L"50");});
 for(auto value:{L"1",L"50",L"150"})add("CLI-021",storage::encode(value),[&](auto& s){set(s,"frame-buffer-size",value);});
 int n=0;for(auto value:{L"0.150",L"-0.1",L"x1.04+0.1",L"x25.025/24-1:30.5"})add("CLI-022",std::to_string(++n),[&](auto& s){set(s,"time-shift",value);});
 n=0;for(auto value:{L"0.5",L"-1",L"1.7",L"0"})add("CLI-023",std::to_string(++n),[&](auto& s){set(s,"wheel-sensitivity",value);});
 for(auto entry:{std::pair<const char*,const wchar_t*>{"CLI-024",L"bt2020nc:"},{"CLI-025",L"pc:tv"},{"CLI-026",L"bt2020:bt709"},{"CLI-027",L"smpte2084:"},{"CLI-029",L"850"},{"CLI-030",L"10000"},{"CLI-031",L"2:1.5"},{"CLI-032",L"scale=1920:-2"},{"CLI-033",L"format=gray"},{"CLI-034",L"hflip"},{"CLI-036",L"max"},{"CLI-037",L"native"},{"CLI-051",L"h264"},{"CLI-052",L"h264:trust_dec_pts=1"},{"CLI-053",L"h264"}})add(entry.first,"value",[&](auto& s){set(s,entry.first,entry.second);});
 add("CLI-036","canvas",[](auto& s){set(s,"conversion-size",L"1920x1440");});add("CLI-037","stretch",[](auto& s){set(s,"conversion-fit",L"stretch");});
 for(auto value:{L"auto",L"off",L"on",L"rel",L"auto:off",L":rel"}){std::wstring v=value;std::replace(v.begin(),v.end(),L':',L'-');add("CLI-028",storage::encode(v),[&](auto& s){set(s,"tone-map-mode",value);s.left.text=(assets/L"pq-tagged.mkv").wstring();},"synthetic-HDR");}
 for(auto entry:{std::pair<const char*,const char*>{"CLI-038","histogram-window"},{"CLI-039","vectorscope-window"},{"CLI-040","waveform-window"}})add(entry.first,"window",[&](auto& s){flag(s,entry.second);});
 for(auto entry:{std::pair<const char*,const wchar_t*>{"CLI-041",L"display_mode=parade:colors_mode=coloronblack"},{"CLI-042",L"mode=color4:envelope=instant+peak"},{"CLI-043",L"graticule=orange:display=stack:scale=ire"},{"CLI-044",L"1024x256"}})add(entry.first,"options",[&](auto& s){flag(s,"histogram-window");flag(s,"vectorscope-window");flag(s,"waveform-window");set(s,entry.first,entry.second);});
 add("CLI-045","notop",[](auto& s){flag(s,"histogram-window");flag(s,"scope-notop");});
 for(auto id:{"CLI-047","CLI-048","CLI-049"})add(id,"rawvideo",[&](auto& s){if(std::string(id)!="CLI-049")s.left.text=(assets/L"left.rgb").wstring();if(std::string(id)!="CLI-048")s.right_inputs[0].source.text=(assets/L"left.rgb").wstring();set(s,id,L"rawvideo:pixel_format=rgb24,video_size=160x96,framerate=10");});
 add("CLI-051","options-only",[](auto& s){set(s,"decoder",L":strict=experimental");});
 add("CLI-052","bad-decoder",[](auto& s){set(s,"left-decoder",L"p07_no_decoder");},"",true);
 add("CLI-053","av1",[&](auto& s){s.right_inputs[0].source.text=(assets/L"right-av1.mkv").wstring();set(s,"right-decoder",L"libdav1d:export_side_data=film_grain");},"AV1-decoder");
 for(auto id:{"CLI-055","CLI-056","CLI-057"})add(id,"cuda",[&](auto& s){set(s,id,L"cuda:0");},"CUDA-device");
 add("CLI-055","bad-device",[](auto& s){set(s,"hwaccel",L"cuda:99");},"",true);
 add("CLI-059","vmaf",[](auto& s){set(s,"libvmaf-options",L"model=version=vmaf_4k_v0.6.1");},"libvmaf-model",false,"metrics");
 for(auto& def:override_catalog()){
  add(def.id,"override",[&](auto& s){add_right(s,s.right_inputs[0].source);std::wstring value=L"";if(def.key=="filters")value=L"hflip";if(def.key=="color-space")value=L"bt709";if(def.key=="color-range")value=L"pc";if(def.key=="color-primaries")value=L"bt2020";if(def.key=="color-trc")value=L"smpte2084";if(def.key=="decoder")value=L"h264:trust_dec_pts=1";if(def.key=="demuxer")value=L"mov:probesize=100000";if(def.key=="hwaccel")value=L"cuda:0";if(def.key=="tone-map-mode")value=L"rel";if(def.key=="peak-nits")value=L"850";if(def.key=="boost-tone")value=L"1.5";s.right_inputs[0].overrides[def.key]={OverrideMode::Replace,value};},def.key=="hwaccel"?"CUDA-device":"");
  add(def.id,"clear",[&](auto& s){s.right_inputs[0].overrides[def.key]={OverrideMode::Clear,L""};});
 }
 for(int count:{3,11})add("IN-001",std::to_string(count),[&](auto& s){while(s.right_inputs.size()<static_cast<size_t>(count)){auto id=add_right(s,s.right_inputs[0].source);s.right_inputs.back().overrides["filters"]={OverrideMode::Replace,id%2?L"hflip":L"vflip"};}},"",false,"switch");
 add("IN-005","right-reference",[](auto& s){s.right_inputs[0].source={InputKind::Reference,L"__"};});add("IN-005","left-reference",[](auto& s){s.left={InputKind::Reference,L"__"};});
 add("IN-006","append",[](auto& s){set(s,"filters",L"scale=160:96");set(s,"right-filters",L"__,vflip");s.right_inputs[0].overrides["filters"]={OverrideMode::Append,L"hflip"};});
 add("IN-004","image",[&](auto& s){s.left.text=(assets/L"left.png").wstring();s.right_inputs[0].source.text=(assets/L"right.png").wstring();});
 auto special=assets/L"中文 路径 & (P07) #";fs::create_directories(special);
 fs::copy_file(assets/L"left.mp4",special/L"参考.mp4",fs::copy_options::overwrite_existing);
 fs::copy_file(assets/L"right.mp4",special/L"比较.mp4",fs::copy_options::overwrite_existing);
 add("IN-003","unicode",[&](auto& s){s.left.text=(special/L"参考.mp4").wstring();s.right_inputs[0].source.text=(special/L"比较.mp4").wstring();});
 if(fs::exists(L"C:/Windows/Fonts/arial.ttf")){fs::copy_file(L"C:/Windows/Fonts/arial.ttf",special/L"字体.ttf",fs::copy_options::overwrite_existing);add("CLI-008","unicode-font",[&](auto& s){set(s,"font",(special/L"字体.ttf").wstring());});}
 fs::copy_file(assets/L"left.png",assets/L"frame0001.png",fs::copy_options::overwrite_existing);
 add("IN-004","sequence",[&](auto& s){s.left={InputKind::ImageSequence,(assets/L"frame%04d.png").wstring()};});
 add("IN-004","file-protocol",[&](auto& s){s.left={InputKind::Address,L"file:"+(assets/L"left.mp4").generic_wstring()};});
 write_text(assets/L"input.ffconcat",L"ffconcat version 1.0\nfile 'left.mp4'\n");
 add("IN-004","concat-script",[&](auto& s){s.left={InputKind::Script,(assets/L"input.ffconcat").wstring()};set(s,"left-demuxer",L"concat:safe=0");});
 write_text(assets/L"first.opt",L"--frame-buffer-size 10 --mode hstack --bilinear-texture");
 write_text(assets/L"second.opt",L"--frame-buffer-size 20 --mode vstack");
 add("CLI-005","ordered-files",[&](auto& s){s.configuration.files={(assets/L"first.opt").wstring(),(assets/L"second.opt").wstring()};});
 add("CLI-060","automatic-off",[](auto&){});
 add("CLI-060","automatic-on",[](auto& s){s.configuration.load_automatic_file=true;});
 for(auto id:{"RUN-001","RUN-002","RUN-003","RUN-004","RUN-005","RUN-006","RUN-007","RUN-008","RUN-009","RUN-010","RUN-011","RUN-012","RUN-013","RUN-014","RUN-015"})add(id,"keys",[&](auto& s){if(std::string(id)=="RUN-005")while(s.right_inputs.size()<11)add_right(s,s.right_inputs[0].source);if(std::string(id)=="RUN-001"||std::string(id)=="RUN-006")set(s,"display-mode",L"hstack");},"interaction-smoke",false,id);
 return cases;
}
int wmain(int argc,wchar_t** argv){std::cout<<std::unitbuf;if(argc<4)return 2;SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED|COINIT_DISABLE_OLE1DDE);INITCOMMONCONTROLSEX cc{sizeof(cc),ICC_STANDARD_CLASSES|ICC_BAR_CLASSES|ICC_LISTVIEW_CLASSES};InitCommonControlsEx(&cc);
 State state;state.instance=GetModuleHandleW(nullptr);WNDCLASSEXW cls{sizeof(cls)};cls.lpfnWndProc=window_proc;cls.hInstance=state.instance;cls.lpszClassName=kClass;RegisterClassExW(&cls);state.window=CreateWindowExW(0,kClass,L"P07 production GUI integration",WS_OVERLAPPEDWINDOW,0,0,1020,940,nullptr,nullptr,state.instance,&state);
 int failures=0;try{auto root=fs::absolute(argv[3]);fs::create_directories(root);auto cases=make_cases(argv[1],fs::absolute(argv[2]));std::ofstream report(root/L"results.tsv",std::ios::binary);report<<"id\tvariant\tstatus\tgui_exit\tcli_exit\tgui_pngs\tcli_pngs\tframes_equal\tsizes_equal\tcondition\n";
 for(auto& c:cases){if(argc>4){std::string selection=","+storage::encode(argv[4])+",";if(selection.find(","+c.id+",")==std::string::npos)continue;}auto name=c.id+"-"+c.name;std::replace(name.begin(),name.end(),':','-');auto dir=root/name;fs::create_directories(dir);c.session.working_directory=dir.wstring();if(c.id=="CLI-060")write_text(dir/L"video-compare.opt",L"--bilinear-texture --frame-buffer-size 15");storage::save_session((dir/L"session.vcgui").wstring(),c.session);
  const std::set<std::string> safe_actions={"RUN-001","RUN-002","RUN-004","RUN-006","RUN-012","RUN-015"};
  if(c.condition=="interaction-smoke"&&!safe_actions.count(c.action)){report<<c.id<<'\t'<<c.name<<"\tBLOCKED\t0\t0\t0\t0\t0\t0\tmanual-input-required; global input disabled\n";report.flush();std::cout<<"BLOCKED "<<name<<" manual input required\n";continue;}
  try{auto a=run_case(state,c,dir,true);auto b=run_case(state,c,dir,false);int frames=0;bool equal=true;for(auto& file:a.pngs){if(file.first.find(L"osd")!=std::wstring::npos)continue;++frames;auto other=b.pngs.find(file.first);if(other==b.pngs.end()||other->second!=file.second)equal=false;}bool sizes=a.width==b.width&&a.height==b.height&&a.windows==b.windows;
   std::string status=c.failure?(a.code!=0&&a.code==b.code&&!a.timed_out&&!b.timed_out&&a.pngs.empty()&&b.pngs.empty()?"PASS":"FAIL"):(a.code==0&&b.code==0&&a.window&&b.window&&frames>=2&&equal&&sizes?"PASS":!c.condition.empty()&&a.code!=0&&a.code==b.code?"BLOCKED":"FAIL");
   auto both_have=[&](const wchar_t* value){return a.log.find(value)!=std::wstring::npos&&b.log.find(value)!=std::wstring::npos;};
   if(status=="PASS"){
    if(!a.action_verified||!b.action_verified||a.steps!=b.steps)status="FAIL";
    if(c.condition=="CUDA-device"&&!both_have(L"(cuda)"))status="FAIL";
    if(c.action=="metrics"&&!both_have(L"VMAF("))status="FAIL";
    if(c.action=="switch")for(size_t i=1;i<=c.session.right_inputs.size();++i){auto expected=L"Active right video: "+std::to_wstring(i)+L"/"+std::to_wstring(c.session.right_inputs.size());if(!both_have(expected.c_str()))status="FAIL";}
    if((c.id=="CLI-038"||c.id=="CLI-039"||c.id=="CLI-040"||c.id=="CLI-045")&&a.windows!=2)status="FAIL";
    if((c.id=="CLI-041"||c.id=="CLI-042"||c.id=="CLI-043"||c.id=="CLI-044")&&a.windows!=4)status="FAIL";
    if(c.id=="CLI-005"&&(!both_have(L"Display mode:          split")||!both_have(L"Bilinear filtering:    true")))status="FAIL";
    if(c.id=="CLI-060"&&!both_have(c.name=="automatic-on"?L"Bilinear filtering:    true":L"Bilinear filtering:    false"))status="FAIL";
    if(c.action=="RUN-006"&&!both_have(L"SIGNED DIVERGING"))status="FAIL";
    if(c.action=="RUN-008"&&(!both_have(L"Metrics:")||!both_have(L"Display state:")))status="FAIL";
    if(c.action=="RUN-010"&&(!both_have(L"Display mode set to")||!both_have(L"Aspect view mode set to")))status="FAIL";
    if(c.action=="RUN-012"&&(a.caption.find(L"scopes-open=4")==std::wstring::npos||b.caption.find(L"scopes-open=4")==std::wstring::npos||a.caption.find(L"scopes-closed=1")==std::wstring::npos||b.caption.find(L"scopes-closed=1")==std::wstring::npos))status="FAIL";
    if(c.action=="RUN-013"&&(!both_have(L"Saved window size")||!both_have(L"Restored saved window size")||!both_have(L"Restored startup window size")))status="FAIL";
    if(c.action=="RUN-005")for(int i=1;i<=10;++i){auto target=L"Active right video: "+std::to_wstring(i)+L"/11";if(!both_have(target.c_str()))status="FAIL";}
    if(c.action=="RUN-015"&&(a.caption.find(L"escape-exited=true")==std::wstring::npos||b.caption.find(L"escape-exited=true")==std::wstring::npos))status="FAIL";
   }
   if(status=="FAIL")++failures;report<<c.id<<'\t'<<c.name<<'\t'<<status<<'\t'<<a.code<<'\t'<<b.code<<'\t'<<a.pngs.size()<<'\t'<<b.pngs.size()<<'\t'<<equal<<'\t'<<sizes<<'\t'<<c.condition<<'\n';report.flush();std::cout<<status<<" "<<name<<" png="<<a.pngs.size()<<"/"<<b.pngs.size()<<" exit="<<a.code<<"/"<<b.code<<'\n';
  }catch(const std::exception& e){++failures;write_text(dir/L"error.txt",storage::decode(e.what()));report<<c.id<<'\t'<<c.name<<"\tFAIL\t0\t0\t0\t0\t0\t0\t"<<e.what()<<'\n';report.flush();std::cout<<"FAIL "<<name<<" "<<e.what()<<'\n';if(state.run.active()){state.run.force_stop();while(state.run.active())pump(30);}}
 }
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';++failures;}DestroyWindow(state.window);CoUninitialize();std::cout<<"P07 failures="<<failures<<'\n';return failures?1:0;
}
