// Compile the production window procedures into a console test host so HDROP and
// WM_NOTIFY payloads belong to the receiving process. The installed EXE is tested
// separately by verify-inputs.ps1; this host does not substitute for that check.
#include "../src/main.cpp"
#include <shlobj.h>
#include <fstream>
#include <iostream>

namespace {
void require(bool pass, const char* name) {
  if (!pass) throw std::runtime_error(name);
  std::cout << "PASS: " << name << '\n';
}
void drop(HWND target, const std::vector<std::wstring>& files) {
  std::wstring names;
  for (const auto& path : files) { names += path; names += L'\0'; }
  names += L'\0';
  auto memory = GlobalAlloc(GHND, sizeof(DROPFILES) + names.size() * sizeof(wchar_t));
  if (!memory) throw std::runtime_error("HDROP allocation failed");
  auto* data = static_cast<DROPFILES*>(GlobalLock(memory));
  data->pFiles = sizeof(DROPFILES); data->fWide = TRUE;
  memcpy(reinterpret_cast<char*>(data) + sizeof(DROPFILES), names.data(), names.size() * sizeof(wchar_t));
  GlobalUnlock(memory);
  SendMessageW(target, WM_DROPFILES, reinterpret_cast<WPARAM>(memory), 0);
  // The production drop handler owns DragFinish.
}
std::wstring picker_text;
bool cancel_picker{}, picker_seen{};
HWND test_owner{};
BOOL CALLBACK find_filename(HWND child, LPARAM result) {
  wchar_t type[64]{}, parent_type[64]{};
  GetClassNameW(child, type, 64); GetClassNameW(GetParent(child), parent_type, 64);
  if (std::wstring(type) == L"Edit" && std::wstring(parent_type) == L"ComboBox") {
    *reinterpret_cast<HWND*>(result) = child; return FALSE;
  }
  return TRUE;
}
BOOL CALLBACK drive_picker(HWND window, LPARAM timer) {
  DWORD process{}; GetWindowThreadProcessId(window, &process);
  wchar_t cls[64]{}; GetClassNameW(window, cls, 64);
  if (process != GetCurrentProcessId() || std::wstring(cls) != L"#32770" || GetWindow(window, GW_OWNER) != test_owner) return TRUE;
  HWND filename{}; EnumChildWindows(window, find_filename, reinterpret_cast<LPARAM>(&filename));
  if (!cancel_picker && !filename) return TRUE;
  picker_seen = true;
  if (!cancel_picker) SetWindowTextW(filename, picker_text.c_str());
  KillTimer(nullptr, static_cast<UINT_PTR>(timer));
  PostMessageW(window, WM_COMMAND, cancel_picker ? IDCANCEL : IDOK, 0);
  return FALSE;
}
void CALLBACK picker_timer(HWND, UINT, UINT_PTR timer, DWORD) { EnumWindows(drive_picker, static_cast<LPARAM>(timer)); }
}

int wmain(int argc, wchar_t** argv) {
  std::cout << std::unitbuf;
  if (argc != 2 || !std::filesystem::path(argv[1]).is_absolute()) return 2;
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES};
  InitCommonControlsEx(&controls);
  State s; s.instance = GetModuleHandleW(nullptr);
  WNDCLASSEXW cls{sizeof(cls)}; cls.lpfnWndProc = window_proc; cls.hInstance = s.instance; cls.lpszClassName = kClass;
  RegisterClassExW(&cls);
  int result = 0;
  try {
    HWND window = CreateWindowExW(0, kClass, L"P03 native event tests", WS_OVERLAPPEDWINDOW, 0, 0, 1020, 940, nullptr, nullptr, s.instance, &s);
    require(window != nullptr, "production controls created");
    auto control_bounds = [&](int id) { RECT r{}; GetWindowRect(s.at(id), &r); return r; };
    for (const int width : {956, 1020, 1280}) {
      MoveWindow(window, 0, 0, width, 1000, TRUE);
      const auto list = control_bounds(IDC_RIGHT_LIST);
      for (const int id : {IDC_ENGINE_BROWSE, IDC_WORKDIR_BROWSE, IDC_LEFT_BROWSE, IDC_START, IDC_PREVIEW})
        require(control_bounds(id).right == list.right, "resized rows share right edge");
      const auto engine = control_bounds(IDC_ENGINE), browse_button = control_bounds(IDC_ENGINE_BROWSE);
      require(browse_button.left - engine.right == px(12, s.dpi), "path field fills available width with consistent gap");
      int previous_right = control_bounds(IDC_VALIDATE).right;
      for (const int id : {IDC_COPY, IDC_SETTINGS, IDC_SHOW_LOG, IDC_START}) {
        const auto r = control_bounds(id);
        require(r.left - previous_right == px(12, s.dpi), "action buttons have equal gaps");
        previous_right = r.right;
      }
    }
    const auto preview = s.at(IDC_PREVIEW);
    put(preview, L"Short diagnostic");
    require(!(GetWindowLongPtrW(preview, GWL_STYLE) & (WS_HSCROLL | WS_VSCROLL)), "short preview has no scrollbars");
    const std::wstring long_command(5000, L'x');
    put(preview, long_command);
    require(!(GetWindowLongPtrW(preview, GWL_STYLE) & WS_HSCROLL) &&
            (GetWindowLongPtrW(preview, GWL_STYLE) & WS_VSCROLL) &&
            SendMessageW(preview, EM_GETLINECOUNT, 0, 0) > 1 && text(preview) == long_command,
            "long command wraps without changing content and scrolls vertically");
    put(preview, L"Short again");
    require(!(GetWindowLongPtrW(preview, GWL_STYLE) & WS_VSCROLL), "scrollbar disappears when content fits again");
    std::filesystem::path root(argv[1]); root.make_preferred(); std::filesystem::create_directories(root);
    std::vector<std::wstring> paths;
    for (const auto* name : {L"参考 & (#).mp4", L"right-one.mp4", L"right-two.mp4", L"right-three.mp4"}) {
      const auto path = root / name; std::ofstream file(path, std::ios::binary); file << "P03 path fixture, not encoded video"; file.close(); paths.push_back(path.wstring());
    }
    put(s.at(IDC_ENGINE), (root / L"engine.exe").wstring());
    drop(s.at(IDC_LEFT), {paths[0]});
    require(text(s.at(IDC_LEFT)) == paths[0], "drop Unicode file onto left input");
    drop(s.at(IDC_RIGHT_LIST), {paths[1], paths[2], paths[3]});
    require(s.result.ok() && ListView_GetItemCount(s.at(IDC_RIGHT_LIST)) == 3, "multi-file drop creates 1+3 preview");
    drop(s.at(IDC_RIGHT_LIST), {root.wstring(), (root / L"missing.mp4").wstring()});
    require(ListView_GetItemCount(s.at(IDC_RIGHT_LIST)) == 3, "drop ignores directories and missing files");
    const auto original_id = s.session.right_inputs[0].id;
    s.session.right_inputs[0].overrides["filters"] = {OverrideMode::Replace, L"hflip"};
    refresh_list(s);
    NMLISTVIEW drag{}; drag.hdr = {s.at(IDC_RIGHT_LIST), IDC_RIGHT_LIST, LVN_BEGINDRAG}; drag.iItem = 0;
    SendMessageW(window, WM_NOTIFY, IDC_RIGHT_LIST, reinterpret_cast<LPARAM>(&drag));
    RECT bounds{}; ListView_GetItemRect(s.at(IDC_RIGHT_LIST), 2, &bounds, LVIR_BOUNDS);
    POINT point{bounds.left + 100, (bounds.top + bounds.bottom) / 2}; MapWindowPoints(s.at(IDC_RIGHT_LIST), window, &point, 1);
    SendMessageW(window, WM_LBUTTONUP, 0, MAKELPARAM(point.x, point.y));
    require(s.session.right_inputs[2].id == original_id && text(s.at(IDC_PREVIEW)).find(L"right-one.mp4::filters=hflip") != std::wstring::npos,
            "drag notification and drop position preserve stable ID and filter");
    test_owner = window;
    cancel_picker = true; picker_seen = false;
    SetTimer(nullptr, 0, 200, picker_timer);
    auto picked = browse(window, true);
    require(picker_seen && picked.empty(), "native file picker cancellation");
    cancel_picker = false; picker_seen = false;
    picker_text = L"\"" + paths[1] + L"\" \"" + paths[2] + L"\"";
    SetTimer(nullptr, 0, 200, picker_timer);
    picked = browse(window, true);
    require(picker_seen && picked.size() == 2 && picked[0] == paths[1] && picked[1] == paths[2], "native file picker multi-selection");
    DestroyWindow(window);
  } catch (const std::exception& e) {
    std::cerr << "FAIL: " << e.what() << '\n'; result = 1;
    if (IsWindow(s.window)) DestroyWindow(s.window);
  }
  CoUninitialize(); return result;
}
