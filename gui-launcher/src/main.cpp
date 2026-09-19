#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <wrl/client.h>
#include <algorithm>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include "core/model.h"
#include "ui/settings.h"
#include "process/runtime.h"
#include "core/storage.h"
#include "resource.h"

namespace {
using namespace launcher;
using Microsoft::WRL::ComPtr;
constexpr wchar_t kClass[] = L"VideoCompareGUI.MainWindow";
struct Control { HWND window; int x, y, w, h; bool stretch; };
struct State {
  HWND window{}, status{};
  HINSTANCE instance{};
  HFONT font{}, title_font{};
  UINT dpi{96};
  int scroll{};
  bool updating{};
  std::uint64_t dragging{};
  std::vector<Control> controls;
  std::map<int, HWND> ids;
  Session session;
  BuildResult result;
  process::Run run;
  process::Snapshot runtime;
  HWND log_window{};
  bool has_run{}, probing{}, closing{};
  std::wstring logs_root, displayed_tail;
  HWND at(int id) const { auto i = ids.find(id); return i == ids.end() ? nullptr : i->second; }
  ~State() { if (font) DeleteObject(font); if (title_font) DeleteObject(title_font); }
};
int px(int value, UINT dpi) { return MulDiv(value, static_cast<int>(dpi), 96); }
std::wstring text(HWND window) {
  std::wstring result(static_cast<std::size_t>(GetWindowTextLengthW(window)) + 1, L'\0');
  result.resize(GetWindowTextW(window, result.data(), static_cast<int>(result.size()))); return result;
}
void put(HWND window, const std::wstring& value) { SetWindowTextW(window, value.c_str()); }
void update_runtime(State& s);
void close_launcher(State& s);
void show_log(State& s);
int choice(HWND combo) { return static_cast<int>(SendMessageW(combo, CB_GETCURSEL, 0, 0)); }
void choices(HWND combo, std::initializer_list<const wchar_t*> values, int current = 0) {
  for (const auto* value : values) SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
  SendMessageW(combo, CB_SETCURSEL, current, 0);
}
void input_choices(HWND combo, int current = 0) {
  choices(combo, {L"本地文件", L"图片序列", L"网络地址 / 协议", L"脚本", L"引用另一侧（__）"}, current);
}
std::wstring directory() {
  std::wstring buffer(32768, L'\0');
  const auto size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
  if (!size || size >= buffer.size()) throw std::runtime_error("Cannot locate executable.");
  buffer.resize(size);
  const auto parent = std::filesystem::path(buffer).parent_path();
  if (std::filesystem::is_regular_file(parent / L"portable.mode")) return parent.wstring();
  auto candidate = parent;
  for (int i = 0; i < 6; ++i) {
    if (candidate.filename() == L"gui-launcher") return candidate.wstring();
    if (candidate == candidate.parent_path()) break;
    candidate = candidate.parent_path();
  }
  return parent.wstring();
}
std::vector<std::wstring> browse(HWND owner, bool multiple, bool folder = false, bool executable = false) {
  ComPtr<IFileOpenDialog> dialog;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))))
    throw std::runtime_error("Cannot open file picker.");
  DWORD flags{}; dialog->GetOptions(&flags);
  flags |= FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR;
  if (multiple) flags |= FOS_ALLOWMULTISELECT;
  if (folder) flags |= FOS_PICKFOLDERS;
  dialog->SetOptions(flags);
  const COMDLG_FILTERSPEC media[] = {{L"视频及图片", L"*.mp4;*.mkv;*.mov;*.avi;*.webm;*.m4v;*.ts;*.mpg;*.png;*.jpg;*.jpeg;*.tif;*.tiff;*.bmp"}, {L"所有文件", L"*.*"}};
  const COMDLG_FILTERSPEC exe[] = {{L"可执行程序", L"*.exe"}, {L"所有文件", L"*.*"}};
  if (!folder) dialog->SetFileTypes(2, executable ? exe : media);
  dialog->SetTitle(folder ? L"选择工作目录" : executable ? L"选择 video-compare.exe" : multiple ? L"添加右侧文件（可多选）" : L"选择文件");
  const HRESULT hr = dialog->Show(owner);
  if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) return {};
  if (FAILED(hr)) throw std::runtime_error("File picker failed.");
  ComPtr<IShellItemArray> items;
  if (FAILED(dialog->GetResults(&items))) throw std::runtime_error("Cannot read selection.");
  DWORD count{}; items->GetCount(&count);
  std::vector<std::wstring> paths;
  for (DWORD i = 0; i < count; ++i) {
    ComPtr<IShellItem> item; PWSTR path = nullptr;
    if (SUCCEEDED(items->GetItemAt(i, &item)) && SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
      paths.emplace_back(path); CoTaskMemFree(path);
    }
  }
  return paths;
}

struct Draft {
  InputSource source;
  InputOverride filter;
  std::map<std::string, InputOverride> overrides;
  const Session* session{};
};
void dialog_enabled(HWND window) {
  const bool ref = choice(GetDlgItem(window, IDC_DIALOG_KIND)) == 4;
  EnableWindow(GetDlgItem(window, IDC_DIALOG_PATH), !ref); EnableWindow(GetDlgItem(window, IDC_DIALOG_BROWSE), !ref);
  const int mode = choice(GetDlgItem(window, IDC_FILTER_MODE));
  EnableWindow(GetDlgItem(window, IDC_FILTER_TEXT), mode == 1 || mode == 2);
}
INT_PTR CALLBACK edit_dialog(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  auto* draft = reinterpret_cast<Draft*>(GetWindowLongPtrW(window, DWLP_USER));
  try {
    if (message == WM_INITDIALOG) {
      draft = reinterpret_cast<Draft*>(lparam); SetWindowLongPtrW(window, DWLP_USER, lparam);
      input_choices(GetDlgItem(window, IDC_DIALOG_KIND), static_cast<int>(draft->source.kind));
      choices(GetDlgItem(window, IDC_FILTER_MODE), {L"继承", L"替换", L"追加", L"清空"}, static_cast<int>(draft->filter.mode));
      put(GetDlgItem(window, IDC_DIALOG_PATH), draft->source.text); put(GetDlgItem(window, IDC_FILTER_TEXT), draft->filter.value);
      dialog_enabled(window); return TRUE;
    }
    if (message != WM_COMMAND || !draft) return FALSE;
    const int id = LOWORD(wparam);
    if (id == IDCANCEL) { EndDialog(window, IDCANCEL); return TRUE; }
    if (id == IDC_OVERRIDE_SETTINGS && draft->session) {
      RightInput input;
      input.source = {static_cast<InputKind>(choice(GetDlgItem(window, IDC_DIALOG_KIND))), text(GetDlgItem(window, IDC_DIALOG_PATH))};
      input.overrides = draft->overrides;
      input.overrides["filters"] = {static_cast<OverrideMode>(choice(GetDlgItem(window, IDC_FILTER_MODE))), text(GetDlgItem(window, IDC_FILTER_TEXT))};
      if (ui::show_overrides(window, GetModuleHandleW(nullptr), *draft->session, input)) {
        draft->overrides = std::move(input.overrides);
        const auto filter = draft->overrides.find("filters");
        draft->filter = filter == draft->overrides.end() ? InputOverride{} : filter->second;
        SendDlgItemMessageW(window, IDC_FILTER_MODE, CB_SETCURSEL, static_cast<WPARAM>(draft->filter.mode), 0);
        put(GetDlgItem(window, IDC_FILTER_TEXT), draft->filter.value);
        dialog_enabled(window);
      }
      return TRUE;
    }
    if (id == IDC_DIALOG_BROWSE) {
      const auto paths = browse(window, false);
      if (!paths.empty()) { put(GetDlgItem(window, IDC_DIALOG_PATH), paths[0]); SendDlgItemMessageW(window, IDC_DIALOG_KIND, CB_SETCURSEL, 0, 0); }
      return TRUE;
    }
    if (HIWORD(wparam) == CBN_SELCHANGE) { dialog_enabled(window); return TRUE; }
    if (id == IDOK) {
      Draft edited{{static_cast<InputKind>(choice(GetDlgItem(window, IDC_DIALOG_KIND))), text(GetDlgItem(window, IDC_DIALOG_PATH))},
                   {static_cast<OverrideMode>(choice(GetDlgItem(window, IDC_FILTER_MODE))), text(GetDlgItem(window, IDC_FILTER_TEXT))}};
      if (edited.source.kind == InputKind::Reference) edited.source.text = L"__";
      std::wstring error;
      if (edited.source.text.empty()) error = L"请输入路径或地址，或选择引用另一侧。";
      if (edited.source.text.find(L"::") != std::wstring::npos ||
          ((edited.filter.mode == OverrideMode::Replace || edited.filter.mode == OverrideMode::Append) && edited.filter.value.find(L"::") != std::wstring::npos))
        error = L"右侧输入及滤镜不能包含引擎保留分隔符 ::。";
      if (edited.filter.mode == OverrideMode::Append && (edited.filter.value.empty() || edited.filter.value.find(L"__") != std::wstring::npos))
        error = L"追加内容必须非空且不含 __；专家表达式请使用替换。";
      if (!error.empty()) { put(GetDlgItem(window, IDC_DIALOG_ERROR), error); return TRUE; }
      edited.overrides = draft->overrides;
      edited.overrides["filters"] = edited.filter;
      edited.session = draft->session;
      *draft = std::move(edited); EndDialog(window, IDOK); return TRUE;
    }
  } catch (...) { put(GetDlgItem(window, IDC_DIALOG_ERROR), L"无法完成操作，请检查输入或系统资源。"); }
  return FALSE;
}
std::vector<int> selected_rows(const State& s) {
  std::vector<int> rows; int row = -1;
  while ((row = ListView_GetNextItem(s.at(IDC_RIGHT_LIST), row, LVNI_SELECTED)) != -1) rows.push_back(row);
  return rows;
}
void enable_actions(State& s) {
  const auto rows = selected_rows(s); const bool one = rows.size() == 1;
  EnableWindow(s.at(IDC_EDIT_INPUT), one); EnableWindow(s.at(IDC_DUPLICATE), one); EnableWindow(s.at(IDC_REMOVE), !rows.empty());
  EnableWindow(s.at(IDC_MOVE_UP), one && rows[0] > 0);
  EnableWindow(s.at(IDC_MOVE_DOWN), one && rows[0] + 1 < static_cast<int>(s.session.right_inputs.size()));
}
void refresh_list(State& s, std::uint64_t selected = 0) {
  s.updating = true; const HWND list = s.at(IDC_RIGHT_LIST);
  SendMessageW(list, WM_SETREDRAW, FALSE, 0); ListView_DeleteAllItems(list);
  for (std::size_t i = 0; i < s.session.right_inputs.size(); ++i) {
    const auto& input = s.session.right_inputs[i]; auto number = std::to_wstring(i + 1);
    LVITEMW item{}; item.mask = LVIF_TEXT | LVIF_PARAM; item.iItem = static_cast<int>(i); item.pszText = number.data(); item.lParam = static_cast<LPARAM>(input.id);
    ListView_InsertItem(list, &item);
    auto path = input.source.kind == InputKind::Reference ? L"引用左侧参考（__）" : input.source.text;
    ListView_SetItemText(list, item.iItem, 1, path.data());
    std::wstring summary = L"继承公共设置";
    const auto filter = input.overrides.find("filters");
    if (filter != input.overrides.end() && filter->second.mode != OverrideMode::Inherit) {
      const auto& v = filter->second;
      summary = v.mode == OverrideMode::Clear ? L"清空用户滤镜" : (v.mode == OverrideMode::Append ? L"追加：" : L"替换：") + v.value;
    }
    const auto others = std::count_if(input.overrides.begin(), input.overrides.end(), [](const auto& setting) {
      return setting.first != "filters" && setting.second.mode != OverrideMode::Inherit;
    });
    if (others) summary += L"；另有 " + std::to_wstring(others) + L" 项独立设置";
    ListView_SetItemText(list, item.iItem, 2, summary.data()); ListView_SetCheckState(list, item.iItem, input.enabled);
    if (input.id == selected) ListView_SetItemState(list, item.iItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
  }
  SendMessageW(list, WM_SETREDRAW, TRUE, 0); InvalidateRect(list, nullptr, TRUE); s.updating = false; enable_actions(s);
}
void refresh_preview(State& s) {
  KillTimer(s.window, 1);
  s.session.engine_path = text(s.at(IDC_ENGINE)); s.session.working_directory = text(s.at(IDC_WORKDIR));
  s.session.left = {static_cast<InputKind>(choice(s.at(IDC_LEFT_KIND))), text(s.at(IDC_LEFT))};
  if (s.session.left.kind == InputKind::Reference) s.session.left.text = L"__";
  EnableWindow(s.at(IDC_LEFT), s.session.left.kind != InputKind::Reference); EnableWindow(s.at(IDC_LEFT_BROWSE), s.session.left.kind != InputKind::Reference);
  std::wstring error;
  const wchar_t* layouts[] = {L"split", L"hstack", L"vstack"}; const wchar_t* loops[] = {L"off", L"on", L"pp"};
  set_option(s.session, "display-mode", std::wstring(layouts[std::clamp(choice(s.at(IDC_LAYOUT)), 0, 2)]), error);
  set_option(s.session, "auto-loop-mode", std::wstring(loops[std::clamp(choice(s.at(IDC_LOOP)), 0, 2)]), error);
  const int mode = choice(s.at(IDC_WINDOW_MODE)); unset_option(s.session, "window-size"); unset_option(s.session, "window-fit-display");
  EnableWindow(s.at(IDC_WIDTH), mode == 2); EnableWindow(s.at(IDC_HEIGHT), mode == 2);
  if (mode == 1) set_option(s.session, "window-fit-display", true, error);
  if (mode == 2) set_option(s.session, "window-size", text(s.at(IDC_WIDTH)) + L"x" + text(s.at(IDC_HEIGHT)), error);
  for (const auto& bind : {std::pair<int,const char*>{IDC_DIFFERENCE,"subtraction-mode"}, {IDC_HIGH_DPI,"high-dpi"}, {IDC_TEN_BIT,"10-bpc"}, {IDC_FULLSCREEN,"fullscreen"}})
    set_option(s.session, bind.second, Button_GetCheck(s.at(bind.first)) == BST_CHECKED, error);
  const auto time = text(s.at(IDC_TIMESHIFT));
  if (time.empty()) unset_option(s.session, "time-shift"); else set_option(s.session, "time-shift", time, error);
  s.result = build_comparison(s.session,true);
  std::wstring preview;
  if (s.result.ok()) preview = s.result.plan->preview;
  else { preview = L"请先修正以下设置：\r\n"; for (const auto& d : s.result.diagnostics) preview += L"• " + d.message + L"\r\n"; }
  put(s.at(IDC_PREVIEW), preview); EnableWindow(s.at(IDC_COPY), s.result.ok());
  const bool engine_exists = GetFileAttributesW(s.session.engine_path.c_str()) != INVALID_FILE_ATTRIBUTES;
  EnableWindow(s.at(IDC_START), s.result.ok() && engine_exists && !s.run.active());
  EnableWindow(s.at(IDC_ENGINE_PROBE), engine_exists && !s.run.active());
  put(s.at(IDC_FEEDBACK), s.result.ok() ? (engine_exists ? L"参数就绪。启动成功不等于视频已加载；可先检查引擎。" : L"请通过“选择程序…”指定已有的 video-compare.exe，并保留配套 DLL。") : L"参数尚未就绪：" + s.result.diagnostics.front().message);
  const auto enabled = std::count_if(s.session.right_inputs.begin(), s.session.right_inputs.end(), [](const auto& i) { return i.enabled; });
  const auto status = L"  右侧 " + std::to_wstring(s.session.right_inputs.size()) + L" 项 · 已启用 " + std::to_wstring(enabled) + L" 项 · " + (s.has_run ? s.runtime.message : L"就绪");
  SendMessageW(s.status, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(status.c_str()));
}
void schedule(State& s) { if (!s.updating) SetTimer(s.window, 1, 180, nullptr); }
void layout_log(HWND window) {
  RECT r{};GetClientRect(window,&r);const int dpi=static_cast<int>(GetDpiForWindow(window));
  auto p=[dpi](int n){return MulDiv(n,dpi,96);};
  MoveWindow(GetDlgItem(window,IDC_RUN_STATE),p(12),p(10),r.right-p(24),p(64),TRUE);
  MoveWindow(GetDlgItem(window,IDC_RUN_OUTPUT),p(12),p(80),r.right-p(24),std::max(p(80),static_cast<int>(r.bottom)-p(132)),TRUE);
  const int ids[]={IDC_RUN_STOP,IDC_RUN_FORCE,IDC_RUN_FOLDER,IDC_RUN_FOLLOW,IDCANCEL};
  const int positions[]={12,132,252,404,548},widths[]={110,110,142,134,154};
  for(int i=0;i<5;++i)MoveWindow(GetDlgItem(window,ids[i]),p(positions[i]),r.bottom-p(40),p(widths[i]),p(28),TRUE);
}
INT_PTR CALLBACK log_proc(HWND window,UINT message,WPARAM wp,LPARAM lp) {
  auto* s=reinterpret_cast<State*>(GetWindowLongPtrW(window,DWLP_USER));
  if(message==WM_INITDIALOG) {
    s=reinterpret_cast<State*>(lp);SetWindowLongPtrW(window,DWLP_USER,lp);s->log_window=window;
    Button_SetCheck(GetDlgItem(window,IDC_RUN_FOLLOW),BST_CHECKED);SendDlgItemMessageW(window,IDC_RUN_OUTPUT,EM_SETLIMITTEXT,262144,0);layout_log(window);return TRUE;
  }
  if(!s)return FALSE;
  if(message==WM_SIZE){layout_log(window);return TRUE;}
  if(message==WM_GETMINMAXINFO){auto* info=reinterpret_cast<MINMAXINFO*>(lp);const UINT dpi=GetDpiForWindow(window);info->ptMinTrackSize={px(740,dpi),px(360,dpi)};return TRUE;}
  if(message==WM_CLOSE){ShowWindow(window,SW_HIDE);return TRUE;}
  if(message==WM_DESTROY){s->log_window=nullptr;return TRUE;}
  if(message==WM_COMMAND) {
    switch(LOWORD(wp)) {
      case IDCANCEL:ShowWindow(window,SW_HIDE);return TRUE;
      case IDC_RUN_STOP:s->run.request_stop();return TRUE;
      case IDC_RUN_FORCE:
        if(s->runtime.phase==process::Phase::StopTimedOut&&MessageBoxW(window,L"正常关闭未完成。强制结束本次比较进程？",L"结束比较",MB_YESNO|MB_ICONQUESTION)==IDYES)s->run.force_stop();
        return TRUE;
      case IDC_RUN_FOLDER:if(!s->run.folder().empty())ShellExecuteW(window,L"open",s->run.folder().c_str(),nullptr,nullptr,SW_SHOWNORMAL);return TRUE;
      case IDC_RUN_FOLLOW:s->displayed_tail.clear();update_runtime(*s);return TRUE;
    }
  }
  return FALSE;
}
void show_log(State& s) {
  if(!s.log_window) {
    s.displayed_tail.clear();
    if(!CreateDialogParamW(s.instance,MAKEINTRESOURCEW(IDD_RUN_LOG),s.window,log_proc,reinterpret_cast<LPARAM>(&s)))throw std::runtime_error("Cannot create log window");
  }
  ShowWindow(s.log_window,SW_SHOWNORMAL);update_runtime(s);
}
void update_runtime(State& s) {
  if(s.has_run)s.runtime=s.run.poll();
  if(s.log_window) {
    put(s.log_window,s.probing?L"引擎检查 · 运行日志":L"视频比较 · 运行日志");
    put(GetDlgItem(s.log_window,IDC_RUN_STATE),s.has_run?s.runtime.message+L"  PID="+std::to_wstring(s.runtime.pid)+L"\r\n日志："+s.run.folder():L"尚未启动任务。stdout/stderr 分别标注来源；stderr 不自动等同于错误。");
    if(Button_GetCheck(GetDlgItem(s.log_window,IDC_RUN_FOLLOW))==BST_CHECKED&&s.displayed_tail!=s.runtime.tail) {
      s.displayed_tail=s.runtime.tail;put(GetDlgItem(s.log_window,IDC_RUN_OUTPUT),s.displayed_tail);
      SendDlgItemMessageW(s.log_window,IDC_RUN_OUTPUT,EM_SETSEL,static_cast<WPARAM>(-1),-1);SendDlgItemMessageW(s.log_window,IDC_RUN_OUTPUT,EM_SCROLLCARET,0,0);
    }
    EnableWindow(GetDlgItem(s.log_window,IDC_RUN_STOP),s.runtime.active&&(s.runtime.phase==process::Phase::Starting||s.runtime.phase==process::Phase::Running));
    EnableWindow(GetDlgItem(s.log_window,IDC_RUN_FORCE),s.runtime.active&&s.runtime.phase==process::Phase::StopTimedOut);
    EnableWindow(GetDlgItem(s.log_window,IDC_RUN_FOLDER),!s.run.folder().empty());
  }
  if(s.has_run) {
    const auto status=L"  "+std::wstring(s.probing?L"引擎检查：":L"比较任务：")+s.runtime.message;
    SendMessageW(s.status,SB_SETTEXTW,0,reinterpret_cast<LPARAM>(status.c_str()));
    const bool exists=GetFileAttributesW(s.session.engine_path.c_str())!=INVALID_FILE_ATTRIBUTES;
    EnableWindow(s.at(IDC_START),!s.runtime.active&&s.result.ok()&&exists);EnableWindow(s.at(IDC_ENGINE_PROBE),!s.runtime.active&&exists);
    if(!s.runtime.active){KillTimer(s.window,2);if(s.closing)DestroyWindow(s.window);}
  }
}
void start_run(State& s,bool probe) {
  if(s.run.active()){show_log(s);return;}
  refresh_preview(s);
  const auto result=probe?build_query(s.session.engine_path,s.session.working_directory,"version"):s.result;
  if(!result.ok()){put(s.at(IDC_FEEDBACK),result.diagnostics.front().message);return;}
  std::wstring error;
  try {
    auto plan=probe?*result.plan:storage::prepare_launch(*result.plan,(std::filesystem::path(directory())/L"work"/L"config-cache").wstring());
    if(!s.run.start(plan,s.logs_root,probe?10000:0,error)){put(s.at(IDC_FEEDBACK),error);return;}
  }catch(const std::exception& e){put(s.at(IDC_FEEDBACK),L"无法准备参数文件："+storage::decode(e.what()));return;}
  s.has_run=true;s.probing=probe;s.closing=false;s.displayed_tail.clear();SetTimer(s.window,2,200,nullptr);show_log(s);
}
void close_launcher(State& s) {
  if(!s.run.active()){DestroyWindow(s.window);return;}
  const TASKDIALOG_BUTTON buttons[]={{100,L"保留任务运行并退出 GUI\n独立运行助手继续记录日志"},{101,L"正常结束任务后退出 GUI\n等待引擎关闭；超时后可选择强制结束"},{IDCANCEL,L"取消，返回 GUI"}};
  TASKDIALOGCONFIG config{sizeof(config)};config.hwndParent=s.window;config.dwFlags=TDF_USE_COMMAND_LINKS|TDF_ALLOW_DIALOG_CANCELLATION;
  config.pszWindowTitle=L"关闭启动器";config.pszMainInstruction=L"当前还有任务运行";config.pButtons=buttons;config.cButtons=3;config.nDefaultButton=IDCANCEL;
  int selected_button=IDCANCEL;if(FAILED(TaskDialogIndirect(&config,&selected_button,nullptr,nullptr)))return;
  if(selected_button==100)DestroyWindow(s.window);
  else if(selected_button==101){s.closing=true;s.run.request_stop();show_log(s);}
  else s.closing=false;
}
void layout(State& s) {
  RECT client{}, status{}; GetClientRect(s.window, &client); SendMessageW(s.status, WM_SIZE, 0, 0); GetWindowRect(s.status, &status);
  const int viewport = client.bottom - (status.bottom - status.top), content = px(890, s.dpi);
  s.scroll = std::clamp(s.scroll, 0, std::max(0, content - viewport));
  SCROLLINFO info{sizeof(info), SIF_RANGE | SIF_PAGE | SIF_POS}; info.nMax = content - 1; info.nPage = std::max(1, viewport); info.nPos = s.scroll;
  SetScrollInfo(s.window, SB_VERT, &info, TRUE);
  // Showing/hiding the outer scrollbar changes the usable client width.
  GetClientRect(s.window, &client);
  const int margin = px(24, s.dpi), gap = px(12, s.dpi);
  const int right = client.right - margin, available = right - margin;
  const int actions[] = {IDC_VALIDATE, IDC_COPY, IDC_SETTINGS, IDC_SHOW_LOG, IDC_START};
  for (const auto& c : s.controls) {
    int x = px(c.x, s.dpi), width = c.stretch ? right - x : px(c.w, s.dpi);
    int height = px(c.h, s.dpi);
    const int id = GetDlgCtrlID(c.window);
    if (id == IDC_ENGINE_BROWSE || id == IDC_WORKDIR_BROWSE || id == IDC_LEFT_BROWSE) x = right - width;
    if (id == IDC_ENGINE || id == IDC_WORKDIR || id == IDC_LEFT) width = right - px(116, s.dpi) - gap - x;
    for (int i = 0; i < 5; ++i) if (id == actions[i]) {
      const int space = available - 4 * gap;
      x = margin + space * i / 5 + gap * i;
      width = space * (i + 1) / 5 - space * i / 5;
    }
    if (id == IDC_PREVIEW) height = std::max(height, viewport - px(c.y, s.dpi) - margin);
    MoveWindow(c.window, x, px(c.y, s.dpi) - s.scroll, width, height, TRUE);
  }
  RECT list{}; GetClientRect(s.at(IDC_RIGHT_LIST), &list);
  ListView_SetColumnWidth(s.at(IDC_RIGHT_LIST), 0, px(62, s.dpi));
  ListView_SetColumnWidth(s.at(IDC_RIGHT_LIST), 1, std::max(px(290, s.dpi), static_cast<int>(list.right) * 2 / 3 - px(62, s.dpi)));
  ListView_SetColumnWidth(s.at(IDC_RIGHT_LIST), 2, std::max(px(150, s.dpi), static_cast<int>(list.right) / 3 - px(8, s.dpi)));
  SetWindowPos(s.status, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); InvalidateRect(s.window, nullptr, TRUE);
}
void fonts(State& s) {
  NONCLIENTMETRICSW metrics{sizeof(metrics)};
  if (!SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, s.dpi)) throw std::runtime_error("Cannot get font.");
  metrics.lfMessageFont.lfHeight = -px(15, s.dpi); HFONT normal = CreateFontIndirectW(&metrics.lfMessageFont);
  metrics.lfMessageFont.lfHeight = -px(25, s.dpi); metrics.lfMessageFont.lfWeight = FW_SEMIBOLD;
  HFONT title = CreateFontIndirectW(&metrics.lfMessageFont);
  if (!normal || !title) { if (normal) DeleteObject(normal); if (title) DeleteObject(title); throw std::runtime_error("Cannot create font."); }
  for (const auto& c : s.controls) SendMessageW(c.window, WM_SETFONT, reinterpret_cast<WPARAM>(c.y == 18 ? title : normal), TRUE);
  SendMessageW(s.status, WM_SETFONT, reinterpret_cast<WPARAM>(normal), TRUE);
  if (s.font) DeleteObject(s.font);
  if (s.title_font) DeleteObject(s.title_font);
  s.font = normal; s.title_font = title;
}
void drop_files(State& s, HDROP drop, HWND target) {
  struct Release { HDROP drop; ~Release() { DragFinish(drop); } } release{drop};
  const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0); std::vector<std::wstring> paths;
  for (UINT i = 0; i < count; ++i) {
    std::wstring path(DragQueryFileW(drop, i, nullptr, 0) + 1, L'\0');
    path.resize(DragQueryFileW(drop, i, path.data(), static_cast<UINT>(path.size())));
    const auto attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
    paths.push_back(path);
  }
  if (paths.empty()) return;
  if (target == s.at(IDC_LEFT)) {
    if (paths.size() != 1) { MessageBoxW(s.window, L"左侧仅允许一个参考文件，请将多个文件拖入右侧列表。", L"选择参考", MB_OK); return; }
    put(s.at(IDC_LEFT), paths[0]); SendMessageW(s.at(IDC_LEFT_KIND), CB_SETCURSEL, 0, 0);
  } else {
    std::uint64_t last{}; for (const auto& path : paths) last = add_right(s.session, {InputKind::File, path}); refresh_list(s, last);
  }
  refresh_preview(s);
}
LRESULT CALLBACK drop_subclass(HWND window, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR data) {
  if (message == WM_DROPFILES) {
    auto& s = *reinterpret_cast<State*>(data);
    try { drop_files(s, reinterpret_cast<HDROP>(wparam), window); }
    catch (...) { MessageBoxW(s.window, L"无法添加拖入的文件。", L"添加输入", MB_OK | MB_ICONERROR); }
    return 0;
  }
  return DefSubclassProc(window, message, wparam, lparam);
}
LRESULT CALLBACK preview_subclass(HWND window, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR) {
  const auto result = DefSubclassProc(window, message, wparam, lparam);
  if ((message == WM_SIZE || message == WM_SETTEXT || message == WM_SETFONT) && !GetPropW(window, L"PreviewLayout")) {
    SetPropW(window, L"PreviewLayout", reinterpret_cast<HANDLE>(1));
    const auto first = SendMessageW(window, EM_GETFIRSTVISIBLELINE, 0, 0);
    ShowScrollBar(window, SB_VERT, FALSE);
    RECT area{}; SendMessageW(window, EM_GETRECT, 0, reinterpret_cast<LPARAM>(&area));
    HDC dc = GetDC(window);
    auto font = reinterpret_cast<HFONT>(SendMessageW(window, WM_GETFONT, 0, 0));
    auto previous = font ? SelectObject(dc, font) : nullptr;
    TEXTMETRICW metrics{}; GetTextMetricsW(dc, &metrics);
    if (previous) SelectObject(dc, previous);
    ReleaseDC(window, dc);
    const int lines = static_cast<int>(SendMessageW(window, EM_GETLINECOUNT, 0, 0));
    ShowScrollBar(window, SB_VERT, lines * metrics.tmHeight > area.bottom - area.top);
    SendMessageW(window, EM_LINESCROLL, 0, first - SendMessageW(window, EM_GETFIRSTVISIBLELINE, 0, 0));
    RemovePropW(window, L"PreviewLayout");
  }
  return result;
}
void create_controls(State& s) {
  s.updating = true;
  auto create = [&](int id, const wchar_t* kind, const wchar_t* caption, DWORD style, int x, int y, int w, int h, bool stretch = false) {
    const DWORD ex = std::wstring(kind) == L"EDIT" || std::wstring(kind) == WC_LISTVIEWW ? WS_EX_CLIENTEDGE : 0;
    HWND c = CreateWindowExW(ex, kind, caption, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | style, 0, 0, 0, 0, s.window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), s.instance, nullptr);
    if (!c) throw std::runtime_error("Cannot create control.");
    s.controls.push_back({c,x,y,w,h,stretch}); if (id > 0) s.ids[id] = c;
    if (std::wstring(kind) == L"EDIT") SendMessageW(c, EM_SETLIMITTEXT, 32767, 0); return c;
  };
  auto label = [&](const wchar_t* t, int x, int y, int w, int h = 24) { return create(0,L"STATIC",t,0,x,y,w,h); };
  auto edit = [&](int id, int x, int y, int w, const wchar_t* t = L"") { return create(id,L"EDIT",t,WS_TABSTOP|ES_AUTOHSCROLL,x,y,w,28); };
  auto button = [&](int id, const wchar_t* t, int x, int y, int w) { return create(id,L"BUTTON",t,WS_TABSTOP|BS_PUSHBUTTON,x,y,w,30); };
  auto combo = [&](int id, int x, int y, int w) { return create(id,WC_COMBOBOXW,L"",WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,x,y,w,220); };
  label(L"视频比较",24,18,420,36); label(L"选择一个参考输入与多份待比较输入，集中核对比较参数。",24,62,820);
  label(L"比较程序(&E)",24,102,104); edit(IDC_ENGINE,136,98,632); button(IDC_ENGINE_BROWSE,L"选择程序…",778,98,116);
  label(L"工作目录(&W)",24,140,104); edit(IDC_WORKDIR,136,136,632); button(IDC_WORKDIR_BROWSE,L"选择目录…",778,136,116);
  label(L"左侧参考(&L)",24,190,104); input_choices(combo(IDC_LEFT_KIND,136,186,230));
  button(IDC_ENGINE_PROBE,L"检查引擎",394,186,128);
  edit(IDC_LEFT,136,222,632); button(IDC_LEFT_BROWSE,L"选择文件…",778,222,116);
  label(L"右侧待比较输入 · 可拖入文件，勾选启用，双击编辑；拖动行可排序",24,268,870);
  button(IDC_ADD_FILES,L"添加文件…",24,300,118); button(IDC_ADD_INPUT,L"添加路径 / 地址…",150,300,156);
  button(IDC_EDIT_INPUT,L"编辑…",314,300,92); button(IDC_DUPLICATE,L"复制",414,300,78); button(IDC_REMOVE,L"移除",500,300,78);
  button(IDC_MOVE_UP,L"上移",586,300,78); button(IDC_MOVE_DOWN,L"下移",672,300,78);
  HWND list = create(IDC_RIGHT_LIST,WC_LISTVIEWW,L"",WS_TABSTOP|LVS_REPORT|LVS_SHOWSELALWAYS,24,340,870,174,true);
  ListView_SetExtendedListViewStyle(list,LVS_EX_CHECKBOXES|LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_LABELTIP);
  const wchar_t* columns[] = {L"序号",L"路径 / 地址",L"条目配置"};
  for(int i=0;i<3;++i) { LVCOLUMNW c{}; c.mask=LVCF_TEXT|LVCF_WIDTH; c.pszText=const_cast<PWSTR>(columns[i]); c.cx=100; ListView_InsertColumn(list,i,&c); }
  label(L"显示方式",24,536,84); choices(combo(IDC_LAYOUT,110,532,190),{L"分割对比",L"水平排列",L"垂直排列"});
  label(L"循环方式",330,536,84); choices(combo(IDC_LOOP,416,532,270),{L"连续播放（不循环）",L"缓冲区正向循环（默认 50 帧）",L"缓冲区往返循环（默认 50 帧）"});
  label(L"窗口大小",24,574,84); choices(combo(IDC_WINDOW_MODE,110,570,190),{L"默认",L"适应可用屏幕",L"自定义"});
  label(L"宽",326,574,28); edit(IDC_WIDTH,356,570,92,L"1280"); label(L"高",464,574,28); edit(IDC_HEIGHT,494,570,92,L"720"); label(L"可留空一边，自动保持比例",610,574,282);
  create(IDC_DIFFERENCE,L"BUTTON",L"差异视图",WS_TABSTOP|BS_AUTOCHECKBOX,24,610,136,28);
  create(IDC_HIGH_DPI,L"BUTTON",L"高 DPI",WS_TABSTOP|BS_AUTOCHECKBOX,176,610,108,28);
  create(IDC_TEN_BIT,L"BUTTON",L"10 位色深",WS_TABSTOP|BS_AUTOCHECKBOX,308,610,136,28);
  create(IDC_FULLSCREEN,L"BUTTON",L"全屏启动",WS_TABSTOP|BS_AUTOCHECKBOX,468,610,136,28);
  label(L"时间偏移",24,650,84); edit(IDC_TIMESHIFT,110,646,214); label(L"秒数或倍率表达式，例如 -0.080、x25/24+0.1；留空使用默认值",342,650,550);
  button(IDC_VALIDATE,L"校验与预览",24,688,134); button(IDC_COPY,L"复制进程参数",172,688,146);
  button(IDC_SETTINGS,L"全部设置…",334,688,152);
  button(IDC_SHOW_LOG,L"运行日志…",502,688,132);
  button(IDC_START,L"开始比较",684,688,210); EnableWindow(s.at(IDC_START),FALSE);
  create(IDC_FEEDBACK,L"STATIC",L"",SS_NOPREFIX,24,728,870,24,true);
  create(IDC_PREVIEW,L"EDIT",L"",WS_TABSTOP|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,24,760,870,106,true);
  SetWindowSubclass(s.at(IDC_PREVIEW),preview_subclass,2,0);
  SendMessageW(s.at(IDC_PREVIEW),EM_SETLIMITTEXT,1048576,0);
  s.status=CreateWindowExW(0,STATUSCLASSNAMEW,L"",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|SBARS_SIZEGRIP,0,0,0,0,s.window,nullptr,s.instance,nullptr);
  if(!s.status) throw std::runtime_error("Cannot create status.");
  const auto root=directory(); s.session=make_session(root);
  std::filesystem::create_directories(std::filesystem::path(root)/L"work");
  s.logs_root=(std::filesystem::path(root)/L"work"/L"runs").wstring();
  for(const auto& candidate : {std::filesystem::path(root)/L"engine"/L"video-compare.exe",std::filesystem::path(root).parent_path()/L"video-compare.exe"})
    if(std::filesystem::is_regular_file(candidate)) { s.session.engine_path=candidate.wstring(); break; }
  put(s.at(IDC_ENGINE),s.session.engine_path); put(s.at(IDC_WORKDIR),s.session.working_directory);
  for(HWND target : {s.at(IDC_LEFT),list}) { DragAcceptFiles(target,TRUE); SetWindowSubclass(target,drop_subclass,1,reinterpret_cast<DWORD_PTR>(&s)); }
  DragAcceptFiles(s.window,TRUE); fonts(s); s.updating=false; enable_actions(s); refresh_preview(s); layout(s);
}
void edit_right(State& s, bool adding) {
  const auto rows=selected_rows(s); if(!adding && rows.size()!=1) return;
  Draft draft; draft.session = &s.session;
  if(!adding) { const auto& input=s.session.right_inputs[rows[0]]; draft.source=input.source; draft.overrides=input.overrides; auto f=input.overrides.find("filters"); if(f!=input.overrides.end()) draft.filter=f->second; }
  if(DialogBoxParamW(s.instance,MAKEINTRESOURCEW(IDD_INPUT_EDIT),s.window,edit_dialog,reinterpret_cast<LPARAM>(&draft))!=IDOK) return;
  const auto id=adding ? add_right(s.session,draft.source) : s.session.right_inputs[rows[0]].id;
  auto& input=*std::find_if(s.session.right_inputs.begin(),s.session.right_inputs.end(),[id](const auto& i){return i.id==id;});
  input.source=std::move(draft.source); input.overrides=std::move(draft.overrides); refresh_list(s,id); refresh_preview(s);
}
void copy_command(State& s) {
  refresh_preview(s); if(!s.result.ok()) return;
  const auto& command=s.result.plan->command_line;
  HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,(command.size()+1)*sizeof(wchar_t)); if(!memory) return;
  void* data=GlobalLock(memory); if(!data) {GlobalFree(memory);return;}
  memcpy(data,command.c_str(),(command.size()+1)*sizeof(wchar_t)); GlobalUnlock(memory);
  if(!OpenClipboard(s.window)) {GlobalFree(memory);return;}
  EmptyClipboard(); if(!SetClipboardData(CF_UNICODETEXT,memory)) GlobalFree(memory); CloseClipboard();
}
std::wstring session_file(HWND owner,bool save,bool opt) {
  ComPtr<IFileDialog> dialog;
  const auto cls=save?CLSID_FileSaveDialog:CLSID_FileOpenDialog;
  if(FAILED(CoCreateInstance(cls,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog))))throw std::runtime_error("Cannot create file dialog");
  DWORD flags{};dialog->GetOptions(&flags);dialog->SetOptions(flags|FOS_FORCEFILESYSTEM|FOS_NOCHANGEDIR|(save?FOS_OVERWRITEPROMPT:FOS_FILEMUSTEXIST));
  COMDLG_FILTERSPEC type{opt?L"引擎参数文件":L"GUI 会话文件",opt?L"*.opt":L"*.vcgui"};dialog->SetFileTypes(1,&type);dialog->SetDefaultExtension(opt?L"opt":L"vcgui");
  dialog->SetTitle(save?L"保存配置":L"打开配置");auto result=dialog->Show(owner);if(result==HRESULT_FROM_WIN32(ERROR_CANCELLED))return {};if(FAILED(result))throw std::runtime_error("File selection failed");
  ComPtr<IShellItem> item;PWSTR path{};if(FAILED(dialog->GetResult(&item))||FAILED(item->GetDisplayName(SIGDN_FILESYSPATH,&path)))throw std::runtime_error("Cannot read selected path");std::wstring out(path);CoTaskMemFree(path);return out;
}
void apply_session(State& s,Session value) {
  s.updating=true;s.session=std::move(value);
  auto string_value=[&](const char* key,const wchar_t* fallback=L""){auto i=s.session.global.find(key);return i!=s.session.global.end()&&std::holds_alternative<std::wstring>(i->second)?std::get<std::wstring>(i->second):std::wstring(fallback);};
  auto flag=[&](const char* key){auto i=s.session.global.find(key);return i!=s.session.global.end()&&std::holds_alternative<bool>(i->second)&&std::get<bool>(i->second);};
  put(s.at(IDC_ENGINE),s.session.engine_path);put(s.at(IDC_WORKDIR),s.session.working_directory);put(s.at(IDC_LEFT),s.session.left.text);SendMessageW(s.at(IDC_LEFT_KIND),CB_SETCURSEL,static_cast<int>(s.session.left.kind),0);
  auto mode=string_value("display-mode",L"split"),loop=string_value("auto-loop-mode",L"off");SendMessageW(s.at(IDC_LAYOUT),CB_SETCURSEL,mode==L"hstack"?1:mode==L"vstack"?2:0,0);SendMessageW(s.at(IDC_LOOP),CB_SETCURSEL,loop==L"on"?1:loop==L"pp"?2:0,0);
  auto size=string_value("window-size");auto x=size.find(L'x');SendMessageW(s.at(IDC_WINDOW_MODE),CB_SETCURSEL,!size.empty()?2:flag("window-fit-display")?1:0,0);put(s.at(IDC_WIDTH),size.empty()?L"1280":size.substr(0,x));put(s.at(IDC_HEIGHT),size.empty()?L"720":x==std::wstring::npos?L"":size.substr(x+1));
  for(auto bind:{std::pair<int,const char*>{IDC_DIFFERENCE,"subtraction-mode"},{IDC_HIGH_DPI,"high-dpi"},{IDC_TEN_BIT,"10-bpc"},{IDC_FULLSCREEN,"fullscreen"}})Button_SetCheck(s.at(bind.first),flag(bind.second)?BST_CHECKED:BST_UNCHECKED);
  put(s.at(IDC_TIMESHIFT),string_value("time-shift"));s.updating=false;refresh_list(s);refresh_preview(s);
}
void configuration_command(State& s,int id) {
  refresh_preview(s);
  try {
    if(id==IDM_CONFIG_INSPECT){put(s.at(IDC_PREVIEW),storage::inspect_sources(s.session));return;}
    const bool save=id==IDM_SESSION_SAVE||id==IDM_OPT_EXPORT,opt=id==IDM_OPT_IMPORT||id==IDM_OPT_EXPORT;
    auto path=session_file(s.window,save,opt);if(path.empty())return;
    if(id==IDM_SESSION_SAVE)storage::save_session(path,s.session);
    else if(id==IDM_SESSION_OPEN){auto draft=storage::load_session(path);apply_session(s,std::move(draft));}
    else if(id==IDM_OPT_EXPORT)storage::export_options(path,s.session);
    else {
      auto imported=storage::import_options(path,s.session);
      if(imported.editable)apply_session(s,std::move(imported.session));
      else if(MessageBoxW(s.window,(imported.report.substr(0,2500)+L"\r\n将此文件添加为兼容引用？").c_str(),L"参数文件兼容模式",MB_YESNO|MB_ICONQUESTION)==IDYES){s.session.configuration.files.push_back(path);refresh_preview(s);}
      put(s.at(IDC_PREVIEW),imported.report);
    }
    put(s.at(IDC_FEEDBACK),L"操作完成："+path);
  }catch(const std::exception& e){put(s.at(IDC_FEEDBACK),L"配置操作失败；未加载损坏文件或覆盖目标："+storage::decode(e.what()));}
}
void on_command(State& s,int id,int notification) {
  if(notification==EN_CHANGE || notification==CBN_SELCHANGE) {schedule(s);return;}
  if(id==IDM_EXIT) {close_launcher(s);return;}
  if(id>=IDM_SESSION_SAVE&&id<=IDM_CONFIG_INSPECT){configuration_command(s,id);return;}
  if(id==IDC_START||id==IDC_ENGINE_PROBE){start_run(s,id==IDC_ENGINE_PROBE);return;}
  if(id==IDC_SHOW_LOG){show_log(s);return;}
  if(id==IDC_SETTINGS) {
    refresh_preview(s);
    if(ui::show_settings(s.window,s.instance,s.session)) {refresh_list(s);refresh_preview(s);}
    return;
  }
  if(id==IDM_ABOUT) {MessageBoxW(s.window,L"Video Compare GUI 0.1.1\r\n\r\n支持多份输入、全部设置、独立引擎启动及运行日志。\r\n请通过“选择程序…”指定 video-compare.exe，并保留其配套 DLL。",L"关于 Video Compare GUI",MB_OK|MB_ICONINFORMATION);return;}
  if(id==IDC_ENGINE_BROWSE || id==IDC_WORKDIR_BROWSE || id==IDC_LEFT_BROWSE || id==IDC_ADD_FILES) {
    auto paths=browse(s.window,id==IDC_ADD_FILES,id==IDC_WORKDIR_BROWSE,id==IDC_ENGINE_BROWSE); if(paths.empty()) return;
    if(id==IDC_ADD_FILES) {std::uint64_t last{};for(const auto& p:paths)last=add_right(s.session,{InputKind::File,p});refresh_list(s,last);}
    else {put(s.at(id==IDC_ENGINE_BROWSE?IDC_ENGINE:id==IDC_WORKDIR_BROWSE?IDC_WORKDIR:IDC_LEFT),paths[0]);if(id==IDC_LEFT_BROWSE)SendMessageW(s.at(IDC_LEFT_KIND),CB_SETCURSEL,0,0);}
    refresh_preview(s);return;
  }
  if(id==IDC_ADD_INPUT || id==IDC_EDIT_INPUT) {edit_right(s,id==IDC_ADD_INPUT);return;}
  const auto rows=selected_rows(s);
  if(id==IDC_DUPLICATE && rows.size()==1) {auto new_id=duplicate_right(s.session,s.session.right_inputs[rows[0]].id);refresh_list(s,new_id);}
  if(id==IDC_REMOVE) {std::vector<std::uint64_t> ids;for(int row:rows)ids.push_back(s.session.right_inputs[row].id);for(auto input:ids)remove_right(s.session,input);refresh_list(s);}
  if((id==IDC_MOVE_UP || id==IDC_MOVE_DOWN) && rows.size()==1) {
    const auto id_to_move=s.session.right_inputs[rows[0]].id;const int target=rows[0]+(id==IDC_MOVE_UP?-1:1);
    if(target>=0 && move_right(s.session,id_to_move,static_cast<std::size_t>(target)))refresh_list(s,id_to_move);
  }
  if(id==IDC_COPY) {copy_command(s);return;}
  refresh_preview(s);if(id==IDC_VALIDATE)SetFocus(s.at(IDC_PREVIEW));
}
LRESULT handle(State& s,UINT message,WPARAM wparam,LPARAM lparam) {
  switch(message) {
    case WM_CREATE: create_controls(s);return 0;
    case WM_CTLCOLORSTATIC: {auto dc=reinterpret_cast<HDC>(wparam);SetTextColor(dc,GetSysColor(COLOR_WINDOWTEXT));SetBkColor(dc,GetSysColor(COLOR_WINDOW));return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));}
    case WM_SIZE: if(wparam!=SIZE_MINIMIZED && s.status)layout(s);return 0;
    case WM_GETMINMAXINFO: {auto* m=reinterpret_cast<MINMAXINFO*>(lparam);m->ptMinTrackSize={px(956,s.dpi),px(600,s.dpi)};return 0;}
    case WM_DPICHANGED: {s.dpi=HIWORD(wparam);fonts(s);auto* r=reinterpret_cast<RECT*>(lparam);SetWindowPos(s.window,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);layout(s);return 0;}
    case WM_VSCROLL: {
      SCROLLINFO i{sizeof(i),SIF_ALL};GetScrollInfo(s.window,SB_VERT,&i);
      switch(LOWORD(wparam)) {
        case SB_LINEUP:s.scroll-=px(36,s.dpi);break;case SB_LINEDOWN:s.scroll+=px(36,s.dpi);break;
        case SB_PAGEUP:s.scroll-=static_cast<int>(i.nPage);break;case SB_PAGEDOWN:s.scroll+=static_cast<int>(i.nPage);break;
        case SB_THUMBTRACK:s.scroll=i.nTrackPos;break;case SB_TOP:s.scroll=0;break;case SB_BOTTOM:s.scroll=i.nMax;break;
      }
      layout(s);return 0;
    }
    case WM_MOUSEWHEEL:s.scroll-=MulDiv(GET_WHEEL_DELTA_WPARAM(wparam),px(72,s.dpi),WHEEL_DELTA);layout(s);return 0;
    case WM_TIMER:if(wparam==1)refresh_preview(s);else if(wparam==2)update_runtime(s);return 0;
    case WM_COMMAND:if(!s.updating)on_command(s,LOWORD(wparam),HIWORD(wparam));return 0;
    case WM_NOTIFY: {
      auto* h=reinterpret_cast<NMHDR*>(lparam);
      if(h->idFrom==IDC_RIGHT_LIST && !s.updating) {
        if(h->code==LVN_ITEMCHANGED) {
          auto* c=reinterpret_cast<NMLISTVIEW*>(lparam);
          if(c->iItem>=0 && c->iItem<static_cast<int>(s.session.right_inputs.size()) && (c->uChanged&LVIF_STATE)) {
            s.session.right_inputs[c->iItem].enabled=ListView_GetCheckState(s.at(IDC_RIGHT_LIST),c->iItem)!=FALSE;enable_actions(s);schedule(s);
          }
        }
        if(h->code==NM_DBLCLK)edit_right(s,false);
        if(h->code==LVN_BEGINDRAG) {auto* d=reinterpret_cast<NMLISTVIEW*>(lparam);if(d->iItem>=0 && d->iItem<static_cast<int>(s.session.right_inputs.size())) {s.dragging=s.session.right_inputs[d->iItem].id;SetCapture(s.window);}}
      }
      return 0;
    }
    case WM_LBUTTONUP:if(s.dragging) {
      const auto id=s.dragging;s.dragging=0;ReleaseCapture();POINT point{GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)};
      MapWindowPoints(s.window,s.at(IDC_RIGHT_LIST),&point,1);LVHITTESTINFO hit{};hit.pt=point;
      const int row=ListView_HitTest(s.at(IDC_RIGHT_LIST),&hit);if(row>=0 && move_right(s.session,id,static_cast<std::size_t>(row))) {refresh_list(s,id);refresh_preview(s);}
    }return 0;
    case WM_CAPTURECHANGED:s.dragging=0;return 0;
    case WM_DROPFILES:drop_files(s,reinterpret_cast<HDROP>(wparam),s.window);return 0;
    case WM_CLOSE:close_launcher(s);return 0;
    case WM_DESTROY:KillTimer(s.window,1);KillTimer(s.window,2);if(s.log_window)DestroyWindow(s.log_window);PostQuitMessage(0);return 0;
  }
  return DefWindowProcW(s.window,message,wparam,lparam);
}
LRESULT CALLBACK window_proc(HWND window,UINT message,WPARAM wparam,LPARAM lparam) {
  auto* s=reinterpret_cast<State*>(GetWindowLongPtrW(window,GWLP_USERDATA));
  if(message==WM_NCCREATE) {s=static_cast<State*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);s->window=window;SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(s));}
  if(!s)return DefWindowProcW(window,message,wparam,lparam);
  if(message==WM_NCDESTROY)SetWindowLongPtrW(window,GWLP_USERDATA,0);
  try{return handle(*s,message,wparam,lparam);}catch(...){MessageBoxW(window,L"无法完成操作，请检查输入或可用系统资源。",L"Video Compare GUI",MB_OK|MB_ICONERROR);return message==WM_CREATE?-1:0;}
}
}  // namespace

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show_command) {
  const HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED|COINIT_DISABLE_OLE1DDE);if(FAILED(com))return 1;
  INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_STANDARD_CLASSES|ICC_BAR_CLASSES|ICC_LISTVIEW_CLASSES};
  if(!InitCommonControlsEx(&controls)){CoUninitialize();return 1;}
  WNDCLASSEXW wc{sizeof(wc)};wc.lpfnWndProc=window_proc;wc.hInstance=instance;wc.hIcon=LoadIconW(nullptr,IDI_APPLICATION);wc.hIconSm=wc.hIcon;
  wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=GetSysColorBrush(COLOR_WINDOW);wc.lpszMenuName=MAKEINTRESOURCEW(IDR_MAIN_MENU);wc.lpszClassName=kClass;
  if(!RegisterClassExW(&wc)){CoUninitialize();return 1;}
  int exit_code=1;
  {
    State s;s.instance=instance;s.dpi=GetDpiForSystem();
    RECT work{}; SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
    const int width=std::min(px(1020,s.dpi),static_cast<int>(work.right-work.left));
    const int height=std::min(px(1000,s.dpi),static_cast<int>(work.bottom-work.top));
    HWND window=CreateWindowExW(WS_EX_CONTROLPARENT,kClass,L"Video Compare GUI — 视频比较启动器",WS_OVERLAPPEDWINDOW|WS_VSCROLL,
      work.left+(work.right-work.left-width)/2,work.top+(work.bottom-work.top-height)/2,width,height,nullptr,nullptr,instance,&s);
    if(window) {
      ShowWindow(window,show_command);UpdateWindow(window);MSG message{};BOOL result;
      while((result=GetMessageW(&message,nullptr,0,0))>0) {
        if(!(s.log_window&&IsWindowVisible(s.log_window)&&IsDialogMessageW(s.log_window,&message))&&!IsDialogMessageW(window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}
        if(message.message==WM_KEYDOWN && message.wParam==VK_TAB) {
          const HWND focus=GetFocus();for(const auto& c:s.controls)if(c.window==focus) {
            RECT r{};GetClientRect(window,&r);const int top=px(c.y,s.dpi),bottom=top+px(std::min(c.h,30),s.dpi);
            int scroll=s.scroll;if(top<scroll)scroll=top;else if(bottom>scroll+r.bottom-px(34,s.dpi))scroll=bottom-r.bottom+px(34,s.dpi);
            if(scroll!=s.scroll){s.scroll=scroll;layout(s);}
          }
        }
      }
      exit_code=result==-1?1:static_cast<int>(message.wParam);
    }
  }
  CoUninitialize();return exit_code;
}
