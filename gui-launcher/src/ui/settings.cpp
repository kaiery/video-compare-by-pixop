#include "settings.h"
#include "resource.h"
#include "core/storage.h"
#include "process/runtime.h"
#include <commctrl.h>
#include <windowsx.h>
#include <shobjidl.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <sstream>
#include <stdexcept>

namespace launcher::ui {
const std::vector<SettingDefinition>& settings_catalog() {
  static const std::vector<SettingDefinition> items{
#include "settings_catalog.inc"
  };
  return items;
}
int setting_control(const std::string& id) { return 2000 + (std::stoi(id.substr(4)) - 1) * 10; }
int override_control(const std::string& id) { return 4000 + (std::stoi(id.substr(3)) - 1) * 10; }
namespace {
using Microsoft::WRL::ComPtr;
std::wstring read(HWND h) {
  std::wstring value(static_cast<std::size_t>(GetWindowTextLengthW(h)) + 1, L'\0');
  value.resize(GetWindowTextW(h, value.data(), static_cast<int>(value.size()))); return value;
}
void write(HWND h, const std::wstring& value) { SetWindowTextW(h, value.c_str()); }
bool checked(HWND h) { return Button_GetCheck(h) == BST_CHECKED; }
int selected(HWND h) { return static_cast<int>(SendMessageW(h, CB_GETCURSEL, 0, 0)); }
std::wstring wide(const std::string& s) { return {s.begin(), s.end()}; }
const wchar_t* scope_name(Scope scope) {
  switch (scope) {
    case Scope::Common: return L"公共输入";
    case Scope::Left: return L"左侧";
    case Scope::Right: return L"右侧默认";
    case Scope::Paired: return L"公共 / 左侧 / 右侧默认";
    case Scope::Config: return L"配置来源";
    case Scope::Query: return L"独立查询";
    default: return L"全局";
  }
}
const wchar_t* override_name(const std::string& key) {
  static const std::map<std::string, const wchar_t*> names{
    {"filters", L"用户滤镜"}, {"color-space", L"色彩矩阵"}, {"color-range", L"色彩范围"},
    {"color-primaries", L"原色"}, {"color-trc", L"传递函数"}, {"decoder", L"解码器"},
    {"demuxer", L"解封装器"}, {"hwaccel", L"硬件加速"}, {"tone-map-mode", L"色调映射模式"},
    {"peak-nits", L"峰值亮度（尼特）"}, {"boost-tone", L"色调增益"}};
  return names.at(key);
}
std::vector<std::wstring> pick(HWND owner, bool font) {
  ComPtr<IFileOpenDialog> dialog;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))))
    throw std::runtime_error("Cannot create file picker");
  DWORD flags{}; dialog->GetOptions(&flags);
  dialog->SetOptions(flags | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR | (font ? 0 : FOS_ALLOWMULTISELECT));
  const COMDLG_FILTERSPEC types[] = {{font ? L"字体" : L"参数文件", font ? L"*.ttf;*.otf;*.ttc" : L"*.opt"}, {L"所有文件", L"*.*"}};
  dialog->SetFileTypes(2, types);
  const HRESULT hr = dialog->Show(owner);
  if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) return {};
  if (FAILED(hr)) throw std::runtime_error("File selection failed");
  ComPtr<IShellItemArray> items;
  if (FAILED(dialog->GetResults(&items))) throw std::runtime_error("Cannot read file selection");
  DWORD count{}; items->GetCount(&count); std::vector<std::wstring> paths;
  for (DWORD i = 0; i < count; ++i) {
    ComPtr<IShellItem> item; PWSTR path{};
    if (SUCCEEDED(items->GetItemAt(i, &item)) && SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
      paths.emplace_back(path); CoTaskMemFree(path);
    }
  }
  return paths;
}
struct Cell { HWND window; int page, y, h, x, width, column, columns; };
struct Row {
  const OptionDefinition* option{};
  const OverrideDefinition* override_def{};
  int page{};
  std::vector<HWND> modes, values;
};
struct Editor {
  HWND window{}, panel{};
  HFONT font{};
  int dpi{96}, page{}, scroll{};
  bool initializing{true};
  Session original, draft;
  bool overrides{};
  RightInput input;
  std::vector<Row> rows;
  std::vector<Cell> cells;
  std::array<int, 10> heights{};
  std::vector<std::wstring> files;
  process::Run query_run;
  std::string query_key;
  std::wstring query_preview;
  ~Editor(){if(query_run.active())query_run.force_stop();}
  HWND at(int id) const {
    if (auto h = GetDlgItem(window, id)) return h;
    return GetDlgItem(panel, id);
  }
  int px(int value) const { return MulDiv(value, dpi, 96); }
};
void layout_panel(Editor& e) {
  RECT bounds{}; GetClientRect(e.panel, &bounds);
  const int height = bounds.bottom;
  const int total = e.px(e.heights[e.page]);
  e.scroll = std::clamp(e.scroll, 0, std::max(0, total - height));
  SCROLLINFO info{sizeof(info), SIF_RANGE | SIF_PAGE | SIF_POS};
  info.nMax = std::max(0, total - 1); info.nPage = std::max(1, height); info.nPos = e.scroll;
  SetScrollInfo(e.panel, SB_VERT, &info, TRUE);
  // Switching pages can add/remove the scrollbar, changing the usable width.
  GetClientRect(e.panel, &bounds);
  const int width = bounds.right;
  for (const auto& c : e.cells) {
    ShowWindow(c.window, c.page == e.page ? SW_SHOWNA : SW_HIDE);
    if (c.page != e.page) continue;
    const int column_width = width / c.columns;
    MoveWindow(c.window, column_width * c.column + e.px(c.x), e.px(c.y) - e.scroll,
               c.width ? e.px(c.width) : std::max(e.px(30), column_width - e.px(c.x + 12)), e.px(c.h), TRUE);
  }
  InvalidateRect(e.panel, nullptr, TRUE);
}
void layout(Editor& e) {
  RECT r{}; GetClientRect(e.window, &r); const int w = r.right, h = r.bottom;
  auto move = [&](int id, int x, int y, int width, int height) { MoveWindow(e.at(id), x, y, width, height, TRUE); };
  move(IDC_SETTINGS_PAGES, e.px(12), e.px(12), e.px(150), h - e.px(210));
  move(IDC_SETTINGS_NOTE, e.px(180), e.px(12), w - e.px(196), e.px(46));
  MoveWindow(e.panel, e.px(176), e.px(62), w - e.px(188), h - e.px(266), TRUE);
  move(IDC_SETTINGS_PREVIEW, e.px(12), h - e.px(192), w - e.px(24), e.px(134));
  move(IDC_SETTINGS_VALIDATE, e.px(12), h - e.px(44), e.px(150), e.px(30));
  move(IDC_SETTINGS_RESET, e.px(174), h - e.px(44), e.px(150), e.px(30));
  move(IDOK, w - e.px(218), h - e.px(44), e.px(94), e.px(30));
  move(IDCANCEL, w - e.px(110), h - e.px(44), e.px(94), e.px(30));
  layout_panel(e);
}
LRESULT CALLBACK panel_proc(HWND window, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR data) {
  auto& e = *reinterpret_cast<Editor*>(data);
  if (msg == WM_COMMAND) return SendMessageW(e.window, msg, wp, lp);
  if (msg == WM_VSCROLL || msg == WM_MOUSEWHEEL) {
    SCROLLINFO info{sizeof(info), SIF_ALL}; GetScrollInfo(window, SB_VERT, &info);
    if (msg == WM_MOUSEWHEEL) e.scroll -= MulDiv(GET_WHEEL_DELTA_WPARAM(wp), e.px(72), WHEEL_DELTA);
    else switch (LOWORD(wp)) {
      case SB_LINEUP: e.scroll -= e.px(32); break;
      case SB_LINEDOWN: e.scroll += e.px(32); break;
      case SB_PAGEUP: e.scroll -= info.nPage; break;
      case SB_PAGEDOWN: e.scroll += info.nPage; break;
      case SB_THUMBTRACK: e.scroll = info.nTrackPos; break;
      case SB_TOP: e.scroll = 0; break;
      case SB_BOTTOM: e.scroll = info.nMax; break;
    }
    layout_panel(e); return 0;
  }
  return DefSubclassProc(window, msg, wp, lp);
}
HWND add(Editor& e, int id, const wchar_t* type, const std::wstring& label, DWORD style,
         int page, int y, int h = 26, int x = 10, int width = 0, int column = 0, int columns = 1) {
  auto window = CreateWindowExW(std::wstring(type) == L"EDIT" || std::wstring(type) == L"LISTBOX" ? WS_EX_CLIENTEDGE : 0,
    type, label.c_str(), WS_CHILD | WS_CLIPSIBLINGS | style, 0, 0, 0, 0, e.panel,
    reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
  if (!window) throw std::runtime_error("Cannot create settings control");
  SendMessageW(window, WM_SETFONT, reinterpret_cast<WPARAM>(e.font), FALSE);
  if (std::wstring(type) == L"EDIT") SendMessageW(window, EM_SETLIMITTEXT, 1048576, 0);
  e.cells.push_back({window, page, y, h, x, width, column, columns}); return window;
}
HWND value_control(Editor& e, int id, const std::wstring& choices, int page, int y, int column = 0, int columns = 1, int x = 130) {
  if (choices.empty()) return add(e, id, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, page, y, 28, x, 0, column, columns);
  auto window = add(e, id, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL, page, y, 180, x, 0, column, columns);
  std::wistringstream stream(choices); std::wstring item;
  while (std::getline(stream, item, L'|')) SendMessageW(window, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.c_str()));
  SendMessageW(window, CB_LIMITTEXT, 1048576, 0); return window;
}
const std::wstring* input_value(const InputOptions& map, const std::string& field) {
  auto found = map.find(field); return found == map.end() ? nullptr : &found->second;
}
void bind_value(Editor& e, Row& row, int base, int column, int columns, int y, const std::wstring* initial, const wchar_t* label) {
  const auto& def = *row.option;
  auto check = add(e, base, L"BUTTON", label, WS_TABSTOP | BS_AUTOCHECKBOX, row.page, y, 26, 10, columns == 1 ? 112 : 0, column, columns);
  auto value = value_control(e, base + 1, def.choices, row.page, columns == 1 ? y : y + 28, column, columns, columns == 1 ? 130 : 10);
  Button_SetCheck(check, initial ? BST_CHECKED : BST_UNCHECKED);
  if (initial) write(value, *initial);
  EnableWindow(value, initial != nullptr); row.modes.push_back(check); row.values.push_back(value);
}
void fill_files(Editor& e, int selection = -1) {
  SendMessageW(e.at(IDC_CONFIG_FILES), LB_RESETCONTENT, 0, 0);
  for (const auto& path : e.files) SendMessageW(e.at(IDC_CONFIG_FILES), LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(path.c_str()));
  SendMessageW(e.at(IDC_CONFIG_FILES), LB_SETCURSEL, selection, 0);
}
void add_options(Editor& e) {
  for (const auto& ui : settings_catalog()) {
    if (ui.main_control) continue;
    const auto* def = find_option(ui.id); if (!def) throw std::runtime_error("Missing option schema");
    const int page = static_cast<int>(ui.page), y = e.heights[page], base = setting_control(ui.id);
    Row row; row.option = def; row.page = page;
    add(e, 0, L"STATIC", std::wstring(ui.label) + L"  ·  " + scope_name(def->scope), SS_NOPREFIX, page, y, 23);
    add(e, 0, L"STATIC", ui.hint, SS_NOPREFIX, page, y + 24, 36);
    if (def->key == "options-file") {
      add(e, IDC_CONFIG_FILES, L"LISTBOX", L"", WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL | WS_HSCROLL, page, y + 66, 100);
      add(e, IDC_CONFIG_PATH, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, page, y + 174, 28);
      const std::pair<int, const wchar_t*> buttons[] = {{IDC_CONFIG_ADD,L"添加路径"},{IDC_CONFIG_BROWSE,L"选择文件…"},{IDC_CONFIG_REMOVE,L"移除"},{IDC_CONFIG_UP,L"上移"},{IDC_CONFIG_DOWN,L"下移"}};
      int x = 10; for (const auto& b : buttons) { add(e,b.first,L"BUTTON",b.second,WS_TABSTOP,page,y+210,28,x,100); x += 110; }
      e.files = e.original.configuration.files; fill_files(e); e.heights[page] += 258;
    } else if (def->scope == Scope::Query) {
      if (def->takes_value()) row.values.push_back(value_control(e, base + 1, L"", page, y + 62));
      add(e, base, L"BUTTON", L"预览查询参数", WS_TABSTOP, page, y + 62, 28, 10, 112);
      add(e, base+2, L"BUTTON", L"执行查询", WS_TABSTOP, page, y + 96, 28, 10, 112);
      add(e, base+3, L"BUTTON", L"取消查询", WS_TABSTOP, page, y + 96, 28, 132, 112);
      e.heights[page] += 34;
      e.heights[page] += 106;
    } else if (def->rule == Rule::Flag) {
      const bool automatic = def->scope == Scope::Config;
      auto h = add(e, base, L"BUTTON", automatic ? L"允许加载自动 .opt（兼容模式）" : L"启用", WS_TABSTOP | BS_AUTOCHECKBOX, page, y + 62, 28);
      auto f = e.original.global.find(def->field);
      const bool on = automatic ? e.original.configuration.load_automatic_file : f != e.original.global.end() && std::get<bool>(f->second);
      Button_SetCheck(h, on ? BST_CHECKED : BST_UNCHECKED); row.modes.push_back(h); e.heights[page] += 106;
    } else if (def->scope == Scope::Paired) {
      const InputOptions* maps[] = {&e.original.common, &e.original.left_options, &e.original.right_defaults};
      const wchar_t* labels[] = {L"指定公共值", L"覆盖左侧", L"覆盖右侧默认"};
      for (int i=0;i<3;++i) bind_value(e,row,base+i*2,i,3,y+62,input_value(*maps[i],def->field),labels[i]);
      e.heights[page] += 132;
    } else {
      const std::wstring* initial{};
      if (def->scope == Scope::Global) { auto f=e.original.global.find(def->field); if(f!=e.original.global.end())initial=std::get_if<std::wstring>(&f->second); }
      else initial=input_value(def->scope==Scope::Common?e.original.common:def->scope==Scope::Left?e.original.left_options:e.original.right_defaults,def->field);
      bind_value(e,row,base,0,1,y+62,initial,L"指定此值"); e.heights[page] += 106;
      if (def->rule==Rule::Font) { add(e,IDC_FONT_BROWSE,L"BUTTON",L"选择字体文件…",WS_TABSTOP,page,y+98,28,130,168);e.heights[page]+=36; }
    }
    e.rows.push_back(std::move(row));
  }
}
void add_overrides(Editor& e) {
  for (const auto& def : override_catalog()) {
    const int y=e.heights[0], base=override_control(def.id);
    Row row;row.override_def=&def;
    add(e,0,L"STATIC",std::wstring(override_name(def.key))+L"  ·  仅当前右侧",SS_NOPREFIX,0,y,23);
    const auto hint=def.allow_append?L"追加仅用于滤镜；清空不关闭引擎自动滤镜。":
      (def.key=="decoder"||def.key=="demuxer"||def.key=="hwaccel")?L"保留引擎字典合并语义；替换名称不等于清除所有继承选项。":L"继承右侧默认；清空表示显式空值。数值与表达式沿用引擎语法。";
    add(e,0,L"STATIC",hint,SS_NOPREFIX,0,y+24,34);
    auto mode=add(e,base,L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,0,y+62,160,10,112);
    int index=0;
    for(const auto& item : {std::pair<OverrideMode,const wchar_t*>{OverrideMode::Inherit,L"继承"},{OverrideMode::Replace,L"替换"},{OverrideMode::Append,L"追加"},{OverrideMode::Clear,L"清空"}}) {
      if(item.first==OverrideMode::Append&&!def.allow_append)continue;
      SendMessageW(mode,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(item.second));SendMessageW(mode,CB_SETITEMDATA,index++,static_cast<LPARAM>(item.first));
    }
    auto value=value_control(e,base+1,def.choices,0,y+62);
    auto found=e.input.overrides.find(def.key);InputOverride initial=found==e.input.overrides.end()?InputOverride{}:found->second;
    for(int i=0;i<index;++i)if(SendMessageW(mode,CB_GETITEMDATA,i,0)==static_cast<LRESULT>(initial.mode))SendMessageW(mode,CB_SETCURSEL,i,0);
    write(value,initial.value);EnableWindow(value,initial.mode==OverrideMode::Replace||initial.mode==OverrideMode::Append);
    row.modes.push_back(mode);row.values.push_back(value);e.rows.push_back(row);e.heights[0]+=106;
  }
}
OverrideMode mode_of(HWND h) { return static_cast<OverrideMode>(SendMessageW(h,CB_GETITEMDATA,selected(h),0)); }
Session collect(Editor& e) {
  Session result=e.original;std::wstring error;
  if(e.overrides) {
    for(const auto& row:e.rows) {
      auto mode=mode_of(row.modes[0]);const auto& key=row.override_def->key;
      if(mode==OverrideMode::Inherit)e.input.overrides.erase(key);
      else e.input.overrides[key]={mode,read(row.values[0])};
    }
    // The preview isolates the current input, with the actual shared and side settings.
    result.right_inputs.clear();auto id=add_right(result,e.input.source);result.right_inputs.back().overrides=e.input.overrides;
    (void)id;return result;
  }
  result.configuration.files=e.files;
  for(const auto& row:e.rows) {
    const auto& def=*row.option;
    if(def.scope==Scope::Query||def.key=="options-file")continue;
    if(def.rule==Rule::Flag) {
      if(def.scope==Scope::Config)result.configuration.load_automatic_file=checked(row.modes[0]);
      else if(checked(row.modes[0]))set_option(result,def.id,true,error);else unset_option(result,def.id);
    } else if(def.scope==Scope::Paired) {
      InputOptions* maps[]={&result.common,&result.left_options,&result.right_defaults};
      for(std::size_t i=0;i<3;++i) {if(checked(row.modes[i]))(*maps[i])[def.field]=read(row.values[i]);else maps[i]->erase(def.field);}
    } else if(checked(row.modes[0]))set_option(result,def.id,read(row.values[0]),error);else unset_option(result,def.id);
    if(!error.empty())throw std::runtime_error("Setting assignment failed");
  }
  return result;
}
std::wstring report(const BuildResult& result) {
  if(result.ok())return result.plan->preview;
  std::wstring message=L"请修正以下设置：\r\n";
  for(const auto& d:result.diagnostics) {
    std::wstring field=wide(d.field);
    if(const auto* option=find_option(d.field)) {
      const auto& definitions=settings_catalog();
      const auto ui=std::find_if(definitions.begin(),definitions.end(),[&](const auto& item){return option->id==item.id;});
      if(ui!=definitions.end())field=std::wstring(ui->label)+L" ("+option->flag+L")";
    }
    message+=field+L"："+d.message+L"\r\n";
  }
  return message;
}
bool validate(Editor& e, bool commit) {
  auto candidate=collect(e);
  // Settings can be edited before input/engine selection. Validate parameter rules
  // using in-memory input placeholders; no fixture is written or executed.
  auto validation=candidate;validation.engine_path=LR"(C:\P04-validation\engine.exe)";validation.working_directory=LR"(C:\P04-validation)";
  validation.left={InputKind::File,L"left.mp4"};
  if(!e.overrides){validation.right_inputs.clear();add_right(validation,{InputKind::File,L"right.mp4"});}
  else validation.right_inputs[0].source={InputKind::File,L"right.mp4"};
  const auto checked_result=build_comparison(validation);
  if(!checked_result.ok()){write(e.at(IDC_SETTINGS_PREVIEW),report(checked_result));return false;}
  const auto actual=build_comparison(candidate);
  write(e.at(IDC_SETTINGS_PREVIEW),actual.ok()?report(actual):L"设置值校验通过；当前会话仍需补齐或修正输入。\r\n"+report(actual));
  if(commit)e.draft=std::move(candidate);return true;
}
void reset_page(Editor& e) {
  for(const auto& row:e.rows)if(row.page==e.page) {
    if(row.override_def)SendMessageW(row.modes[0],CB_SETCURSEL,0,0);
    else for(auto h:row.modes)Button_SetCheck(h,BST_UNCHECKED);
    for(auto h:row.values){write(h,L"");if(!row.option||row.option->scope!=Scope::Query)EnableWindow(h,FALSE);}
  }
  if(!e.overrides&&e.page==static_cast<int>(Page::Config)){e.files.clear();fill_files(e);}
}
void update_modes(Editor& e) {
  for(const auto& row:e.rows) {
    if(row.override_def) {
      const auto mode=mode_of(row.modes[0]);EnableWindow(row.values[0],mode==OverrideMode::Replace||mode==OverrideMode::Append);
    } else if(row.option->scope!=Scope::Query&&!row.values.empty()) {
      for(std::size_t i=0;i<row.values.size();++i)EnableWindow(row.values[i],checked(row.modes[i]));
    }
  }
}
void handle_command(Editor& e,int id,int notification,HWND source) {
  if(id==IDCANCEL){EndDialog(e.window,IDCANCEL);return;}
  if(id==IDOK){if(validate(e,true))EndDialog(e.window,IDOK);return;}
  if(id==IDC_SETTINGS_VALIDATE){validate(e,false);return;}
  if(id==IDC_SETTINGS_RESET){reset_page(e);return;}
  if(id==IDC_SETTINGS_PAGES&&notification==LBN_SELCHANGE) {
    e.page=static_cast<int>(SendMessageW(source,LB_GETCURSEL,0,0));e.scroll=0;layout_panel(e);return;
  }
  if(notification==EN_SETFOCUS||notification==CBN_SETFOCUS) {
    for(const auto& c:e.cells)if(c.window==source&&c.page==e.page) {
      RECT r{};GetClientRect(e.panel,&r);const int top=e.px(c.y),bottom=top+e.px(30);
      if(top<e.scroll)e.scroll=top;else if(bottom>e.scroll+r.bottom)e.scroll=bottom-r.bottom;
      layout_panel(e);break;
    }
  }
  if(notification==BN_CLICKED||notification==CBN_SELCHANGE)update_modes(e);
  if(id==IDC_FONT_BROWSE) {
    auto files=pick(e.window,true);if(!files.empty()){Button_SetCheck(e.at(setting_control("CLI-008")),BST_CHECKED);write(e.at(setting_control("CLI-008")+1),files[0]);update_modes(e);}return;
  }
  if(id==IDC_CONFIG_ADD) {
    auto path=read(e.at(IDC_CONFIG_PATH));if(path.empty()){write(e.at(IDC_SETTINGS_PREVIEW),L"请输入参数文件路径。主窗口文件菜单可导入或查看配置来源。");return;}
    e.files.push_back(path);fill_files(e,static_cast<int>(e.files.size()-1));return;
  }
  if(id==IDC_CONFIG_BROWSE){auto files=pick(e.window,false);e.files.insert(e.files.end(),files.begin(),files.end());fill_files(e);return;}
  if(id==IDC_CONFIG_REMOVE||id==IDC_CONFIG_UP||id==IDC_CONFIG_DOWN) {
    const int index=static_cast<int>(SendMessageW(e.at(IDC_CONFIG_FILES),LB_GETCURSEL,0,0));if(index<0||index>=static_cast<int>(e.files.size()))return;
    if(id==IDC_CONFIG_REMOVE)e.files.erase(e.files.begin()+index);
    else {const int target=index+(id==IDC_CONFIG_UP?-1:1);if(target>=0&&target<static_cast<int>(e.files.size())){std::swap(e.files[index],e.files[target]);fill_files(e,target);return;}}
    fill_files(e,std::min(index,static_cast<int>(e.files.size())-1));return;
  }
  for(const auto& row:e.rows)if(row.option&&row.option->scope==Scope::Query&&(id==setting_control(row.option->id)||id==setting_control(row.option->id)+2||id==setting_control(row.option->id)+3)) {
    const int base=setting_control(row.option->id);
    if(id==base+3){if(e.query_run.active())e.query_run.force_stop();return;}
    const auto result=build_query(e.original.engine_path,e.original.working_directory,row.option->id,
      row.option->takes_value()?std::optional<std::wstring>(read(row.values[0])):std::nullopt);
    if(id==base){write(e.at(IDC_SETTINGS_PREVIEW),report(result));return;}
    if(e.query_run.active()){write(e.at(IDC_SETTINGS_PREVIEW),L"已有查询运行，请先取消或等待。比较任务不受影响。");return;}
    if(!result.ok()){write(e.at(IDC_SETTINGS_PREVIEW),report(result));return;}
    auto root=std::filesystem::path(process::executable_path()).parent_path();auto search=root;
    for(int i=0;i<6;++i){if(search.filename()==L"gui-launcher"){root=search;break;}search=search.parent_path();}
    std::wstring error;
    if(!e.query_run.start(*result.plan,(root/L"work"/L"queries").wstring(),10000,error)){write(e.at(IDC_SETTINGS_PREVIEW),error);return;}
    e.query_key=row.option->key;e.query_preview=report(result);SetTimer(e.window,71,200,nullptr);return;
  }
}
INT_PTR CALLBACK dialog_proc(HWND window,UINT msg,WPARAM wp,LPARAM lp) {
  auto* e=reinterpret_cast<Editor*>(GetWindowLongPtrW(window,DWLP_USER));
  try {
    if(msg==WM_INITDIALOG) {
      e=reinterpret_cast<Editor*>(lp);e->window=window;SetWindowLongPtrW(window,DWLP_USER,lp);
      e->dpi=static_cast<int>(GetDpiForWindow(window));e->font=reinterpret_cast<HFONT>(SendMessageW(window,WM_GETFONT,0,0));
      e->panel=CreateWindowExW(WS_EX_CONTROLPARENT,L"STATIC",L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
        0,0,0,0,window,reinterpret_cast<HMENU>(IDC_SETTINGS_PANEL),GetModuleHandleW(nullptr),nullptr);
      if(!e->panel)throw std::runtime_error("Cannot create settings panel");
      SetWindowSubclass(e->panel,panel_proc,1,reinterpret_cast<DWORD_PTR>(e));
      const wchar_t* pages[]={L"播放与同步",L"画面与窗口",L"色彩 / HDR",L"滤镜",L"解码与硬件",L"分析窗口",L"字体",L"配置与诊断",L"能力查询"};
      if(e->overrides){write(window,L"当前右侧 · 全部 11 项设置");SendMessageW(e->at(IDC_SETTINGS_PAGES),LB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"逐视频设置"));add_overrides(*e);}
      else {for(auto name:pages)SendMessageW(e->at(IDC_SETTINGS_PAGES),LB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));add_options(*e);}
      SendMessageW(e->at(IDC_SETTINGS_PAGES),LB_SETCURSEL,0,0);
      SendMessageW(e->at(IDC_SETTINGS_PREVIEW),EM_SETLIMITTEXT,1048576,0);
      write(e->at(IDC_SETTINGS_NOTE),e->overrides?L"只修改当前条目。继承不会发出覆盖值；追加仅用于滤镜。\r\n下方预览显示此条目与公共设置组合后的参数。":L"未指定的值沿用默认或继承；指定后可保留显式空值。\r\n使用配置文件时，未勾选不能撤销文件内的布尔开关。常用项在主窗口。");
      RECT owner{},work{};GetWindowRect(GetParent(window),&owner);SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
      const int w=std::min(e->px(1080),static_cast<int>(work.right-work.left)),h=std::min(e->px(800),static_cast<int>(work.bottom-work.top));
      SetWindowPos(window,nullptr,std::max(static_cast<int>(work.left),static_cast<int>(owner.left)),std::max(static_cast<int>(work.top),static_cast<int>(work.top+(work.bottom-work.top-h)/2)),w,h,SWP_NOZORDER|SWP_NOACTIVATE);
      layout(*e);e->initializing=false;validate(*e,false);return TRUE;
    }
    if(!e)return FALSE;
    if(msg==WM_TIMER&&wp==71){
      auto state=e->query_run.poll();std::wstring output=state.message+L"\r\n日志："+e->query_run.folder()+L"\r\n"+state.tail+L"\r\n查询参数：\r\n"+e->query_preview;
      if(!state.active){KillTimer(window,71);if(state.phase==process::Phase::Exited&&state.exit_code==0&&e->query_key=="help"){
        process::Utf8Decoder decoder;auto raw=storage::read_bytes((std::filesystem::path(e->query_run.folder())/L"stdout.raw").wstring())+storage::read_bytes((std::filesystem::path(e->query_run.folder())/L"stderr.raw").wstring());
        auto report=storage::compatibility(decoder.feed(raw,true));output=report+output;storage::write_bytes((std::filesystem::path(e->query_run.folder())/L"compatibility.txt").wstring(),storage::encode(report));
      }}write(e->at(IDC_SETTINGS_PREVIEW),output);return TRUE;
    }
    if(msg==WM_SIZE&&!e->initializing){layout(*e);return TRUE;}
    if(msg==WM_GETMINMAXINFO){auto* info=reinterpret_cast<MINMAXINFO*>(lp);info->ptMinTrackSize={e->px(960),e->px(640)};return TRUE;}
    if(msg==WM_COMMAND&&!e->initializing){handle_command(*e,LOWORD(wp),HIWORD(wp),reinterpret_cast<HWND>(lp));return TRUE;}
    if(msg==WM_CLOSE){EndDialog(window,IDCANCEL);return TRUE;}
  } catch(...) {
    if(msg==WM_INITDIALOG){EndDialog(window,-1);return TRUE;}
    if(e)write(e->at(IDC_SETTINGS_PREVIEW),L"操作未完成，请检查输入或可用系统资源。");
  }
  return FALSE;
}
}
bool show_settings(HWND owner,HINSTANCE instance,Session& session) {
  Editor editor;editor.original=session;
  const auto result=DialogBoxParamW(instance,MAKEINTRESOURCEW(IDD_SETTINGS),owner,dialog_proc,reinterpret_cast<LPARAM>(&editor));
  if(result==-1)throw std::runtime_error("Cannot open settings");
  if(result!=IDOK)return false;session=std::move(editor.draft);return true;
}
bool show_overrides(HWND owner,HINSTANCE instance,const Session& session,RightInput& input) {
  Editor editor;editor.original=session;editor.input=input;editor.overrides=true;
  const auto result=DialogBoxParamW(instance,MAKEINTRESOURCEW(IDD_SETTINGS),owner,dialog_proc,reinterpret_cast<LPARAM>(&editor));
  if(result==-1)throw std::runtime_error("Cannot open right-input settings");
  if(result!=IDOK)return false;input.overrides=std::move(editor.input.overrides);return true;
}
}
