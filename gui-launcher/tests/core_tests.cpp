#include <windows.h>
#include <shellapi.h>
#include "core/catalog.h"
#include "core/model.h"
#include <algorithm>
#include <functional>
#include <iostream>
#include <random>
#include <set>
#include <stdexcept>

using namespace launcher;
namespace {
int passed = 0, failed = 0;
void require(bool condition, const std::string& message) { if (!condition) throw std::runtime_error(message); }
void run(const std::string& name, const std::function<void()>& action) {
  try { action(); ++passed; std::cout << "PASS " << name << '\n'; }
  catch (const std::exception& error) { ++failed; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
}
Session fixture() {
  auto session = make_session(LR"(D:\fixture)");
  session.working_directory = LR"(D:\fixture)";
  session.engine_path = LR"(D:\tools\video-compare.exe)";
  session.left = {InputKind::File, L"left.mp4"};
  add_right(session, {InputKind::File, L"right.mp4"});
  return session;
}
void set(Session& session, const std::string& option, const std::wstring& value) {
  std::wstring error;
  require(set_option(session, option, value, error), "set_option rejected " + option);
}
void flag(Session& session, const std::string& option, bool value) {
  std::wstring error;
  require(set_option(session, option, value, error), "set_option rejected flag " + option);
}
std::wstring wide(const std::string& value) { return {value.begin(), value.end()}; }
bool has(const LaunchPlan& plan, const std::wstring& argument) {
  return std::find(plan.arguments.begin(), plan.arguments.end(), argument) != plan.arguments.end();
}
std::wstring option_value(const LaunchPlan& plan, const std::wstring& name) {
  const auto found = std::find(plan.arguments.begin(), plan.arguments.end(), name);
  require(found != plan.arguments.end() && found + 1 != plan.arguments.end(), "missing option/value");
  return *(found + 1);
}
bool diagnostic(const BuildResult& result, const std::string& code) {
  return std::any_of(result.diagnostics.begin(), result.diagnostics.end(), [&](const auto& d) { return d.code == code; });
}
LaunchPlan good(const Session& session) {
  auto result = build_comparison(session);
  if (!result.ok()) {
    std::string codes;
    for (const auto& d : result.diagnostics) codes += d.code + ":" + d.field + " ";
    throw std::runtime_error("build failed: " + codes);
  }
  return *result.plan;
}
void bad(const Session& session, const std::string& code) {
  const auto result = build_comparison(session);
  require(!result.ok() && diagnostic(result, code), "expected rejection " + code);
}

enum class Mode { Flag, Value, QueryFlag, QueryValue };
struct CliCase { const char* id; const char* flag; Mode mode; Scope scope; const wchar_t* value; };
const CliCase cli_cases[]{
#include "cli_cases.inc"
};
struct OverrideCase { const char* id; const char* key; const wchar_t* value; };
const OverrideCase override_cases[]{
#include "override_cases.inc"
};

void catalog_tests() {
  run("catalog.uniqueness-and-coverage", [] {
    require(option_catalog().size() == 61 && std::size(cli_cases) == 61, "CLI coverage size");
    std::set<std::string> ids, keys;
    std::set<std::wstring> flags;
    for (const auto& def : option_catalog()) {
      require(ids.insert(def.id).second && keys.insert(def.key).second, "duplicate id/key");
      require(flags.insert(def.flag).second, "duplicate long flag");
      if (!def.short_flag.empty()) require(flags.insert(def.short_flag).second, "duplicate short flag");
      require(find_option(def.id) == &def && find_option(def.key) == &def, "lookup mismatch");
      require(std::any_of(std::begin(cli_cases), std::end(cli_cases), [&](const auto& c) { return def.id == c.id; }), "missing fixture");
    }
    require(find_option("-c")->key == "show-controls" && find_option("-C")->key == "color-space", "case-sensitive aliases");
    require(find_option("--unknown") == nullptr && find_option("") == nullptr, "unknown lookup");
  });
  for (const auto& c : cli_cases) run(c.id, [&] {
    const auto* def = find_option(c.id);
    require(def && def->flag == wide(c.flag) && def->scope == c.scope, "catalog metadata mismatch");
    auto session = fixture();
    BuildResult result;
    if (c.mode == Mode::QueryFlag || c.mode == Mode::QueryValue) {
      result = build_query(session.engine_path, session.working_directory, c.id,
                           c.mode == Mode::QueryValue ? std::optional<std::wstring>(c.value) : std::nullopt);
    } else {
      if (c.mode == Mode::Flag) flag(session, c.id, true); else set(session, c.id, c.value);
      result = build_comparison(session);
    }
    require(result.ok(), "sample build rejected");
    const auto& plan = *result.plan;
    require(has(plan, wide(c.flag)), "expected flag missing");
    if (c.mode == Mode::Value || c.mode == Mode::QueryValue) require(option_value(plan, wide(c.flag)) == c.value, "value changed");
    require(std::any_of(plan.emissions.begin(), plan.emissions.end(), [&](const auto& e) { return e.id == c.id; }), "trace missing");
    if (c.scope == Scope::Query) {
      require(!has(plan, LR"(D:\fixture\left.mp4)") && !has(plan, L"--"), "query leaked inputs");
    } else {
      require(plan.arguments[plan.arguments.size() - 2] == LR"(D:\fixture\left.mp4)", "left path/order changed");
      require(plan.arguments.back() == LR"(D:\fixture\right.mp4)", "right path/order changed");
      if (c.mode == Mode::Flag) {
        flag(session, c.id, false);
        require(!has(good(session), wide(c.flag)), "false flag still emitted");
      }
      require(unset_option(session, c.id), "cannot restore default");
      if (std::string(c.id) != "CLI-060") require(!has(good(session), wide(c.flag)), "unset option still emitted");
      // Alias setter produces exactly the same canonical argv.
      if (!def->short_flag.empty()) {
        std::string alias;
        for (wchar_t ch : def->short_flag) { require(ch < 128, "non-ASCII alias"); alias.push_back(static_cast<char>(ch)); }
        if (c.mode == Mode::Flag) flag(session, alias, true); else set(session, alias, c.value);
        require(good(session).arguments == plan.arguments, "alias changed argv");
      }
    }
  });
}

void override_tests() {
  run("overrides.coverage", [] {
    require(override_catalog().size() == 11 && std::size(override_cases) == 11, "override count");
    std::set<std::string> keys, ids;
    for (const auto& def : override_catalog()) {
      require(keys.insert(def.key).second && ids.insert(def.id).second, "duplicate override");
      require(find_override(def.id) == &def && find_override(def.key) == &def, "override lookup");
    }
  });
  for (const auto& c : override_cases) run(c.id, [&] {
    const auto* def = find_override(c.id);
    require(def && def->key == c.key, "override mapping");
    auto session = fixture();
    add_right(session, {InputKind::File, L"second.mp4"});
    auto& setting = session.right_inputs[0].overrides[c.key];
    setting = {OverrideMode::Replace, c.value};
    auto plan = good(session);
    const auto expected = std::wstring(LR"(D:\fixture\right.mp4::)") + wide(c.key) + L"=" + c.value;
    require(plan.arguments[plan.arguments.size() - 2] == expected, "incorrect per-video serialization");
    require(plan.arguments.back() == LR"(D:\fixture\second.mp4)", "override contaminated another input");
    require(std::any_of(plan.emissions.begin(), plan.emissions.end(), [&](const auto& e) { return e.id == c.id && e.field == "right[1]." + std::string(c.key); }), "missing stable trace");
    setting.mode = OverrideMode::Clear;
    require(good(session).arguments[plan.arguments.size() - 2] == std::wstring(LR"(D:\fixture\right.mp4::)") + wide(c.key) + L"=", "clear did not emit explicit empty");
    setting.mode = OverrideMode::Inherit;
    require(good(session).arguments[plan.arguments.size() - 2] == LR"(D:\fixture\right.mp4)", "inherit emitted override");
  });
}

void semantic_tests() {
  run("defaults.exact-argv", [] {
    require(good(fixture()).arguments == std::vector<std::wstring>{LR"(D:\tools\video-compare.exe)", L"--no-auto-options-file", L"--", LR"(D:\fixture\left.mp4)", LR"(D:\fixture\right.mp4)"}, "defaults injected unexpected options");
  });
  run("layers.common-left-right-and-three-overrides", [] {
    auto s = fixture();
    set(s, "filters", L"yadif"); set(s, "left-filters", L"__,crop=iw:ih-240"); set(s, "right-filters", L"__,format=gray");
    add_right(s, {InputKind::File, L"second.mp4"}); add_right(s, {InputKind::File, L"third.mp4"});
    s.right_inputs[1].overrides["filters"] = {OverrideMode::Append, L"scale=1920:-1"};
    s.right_inputs[2].overrides["filters"] = {OverrideMode::Clear, L"stale editor text"};
    const auto p = good(s);
    require(option_value(p, L"--filters") == L"yadif", "common lost");
    require(option_value(p, L"--left-filters") == L"__,crop=iw:ih-240", "left expression altered");
    require(option_value(p, L"--right-filters") == L"__,format=gray", "right expression altered");
    require(p.arguments[p.arguments.size()-3] == LR"(D:\fixture\right.mp4)", "inherit wrong");
    require(p.arguments[p.arguments.size()-2] == LR"(D:\fixture\second.mp4::filters=__,scale=1920:-1)", "append wrong");
    require(p.arguments.back() == LR"(D:\fixture\third.mp4::filters=)", "clear wrong");
  });
  run("layers.append-empty-template", [] {
    auto s = fixture(); s.right_inputs[0].overrides["filters"] = {OverrideMode::Append, L"format=gray"};
    require(good(s).arguments.back() == LR"(D:\fixture\right.mp4::filters=format=gray)", "empty template append emitted unresolved __");
  });
  run("layers.paired-side-overrides", [] {
    auto s = fixture(); s.common["color-space"] = L"bt709"; s.right_defaults["color-space"] = L"bt2020nc";
    s.common["boost-tone"] = L"2"; s.left_options["boost-tone"] = L"";
    require(option_value(good(s), L"--color-space") == L"bt709:bt2020nc", "side inheritance");
    require(option_value(good(s), L"--boost-tone") == L":2", "explicit side clear lost");
    set(s, "color-space", L"bt2020");
    require(option_value(good(s), L"--color-space") == L"bt2020", "whole pair assignment left stale side");
  });
  run("layers.peak-mutual-reference", [] {
    auto s = fixture(); set(s, "left-peak-nits", L"__"); set(s, "right-peak-nits", L"850");
    require(option_value(good(s), L"--left-peak-nits") == L"__", "peak reference altered");
    set(s, "right-peak-nits", L"__"); bad(s, "E_PLACEHOLDER");
  });
  run("layers.invalid-references", [] {
    auto s = fixture(); set(s, "left-filters", L"__,scale=1920:-1"); bad(s, "E_PLACEHOLDER");
    s = fixture(); s.right_inputs[0].overrides["filters"] = {OverrideMode::Replace, L"__,format=gray"}; bad(s, "E_PLACEHOLDER");
  });
  run("layers.decoder-dictionaries-preserved", [] {
    auto s = fixture(); set(s, "decoder", L"h264:trust_dec_pts=1"); set(s, "right-decoder", L"__:strict=-2");
    s.right_inputs[0].overrides["decoder"] = {OverrideMode::Replace, L"__:rewrite_duration=1"};
    const auto p = good(s);
    require(option_value(p, L"--decoder") == L"h264:trust_dec_pts=1", "dictionary changed");
    require(p.arguments.back() == LR"(D:\fixture\right.mp4::decoder=__:rewrite_duration=1)", "dictionary override flattened");
  });
  run("validation.unknown-and-wrong-scope", [] {
    auto s = fixture(); s.right_defaults["time-shift"] = L"0.2"; bad(s, "E_SCOPE");
    s = fixture(); s.global["left-filters"] = std::wstring(L"gray"); bad(s, "E_SCOPE");
    s = fixture(); s.right_inputs[0].overrides["time-shift"] = {OverrideMode::Replace, L"0.2"}; bad(s, "E_OVERRIDE_KEY");
    s = fixture(); s.common["peak-nits"] = L"850"; bad(s, "E_SCOPE");
  });
  run("validation.types-and-atomic-setter", [] {
    auto s = fixture(); std::wstring message; const auto before = good(s).arguments;
    require(!set_option(s, "ui-scale", true, message) && !message.empty(), "wrong type accepted");
    require(!set_option(s, "help", true, message), "query accepted into comparison");
    require(!set_option(s, "missing", true, message), "unknown setter");
    require(!set_option(s, "color-space", std::wstring(L"a:b:c"), message), "ambiguous pair");
    require(good(s).arguments == before, "failed setter mutated session");
    s.global["ui-scale"] = true; bad(s, "E_TYPE");
  });
  run("validation.numeric-boundaries", [] {
    for (const auto& c : std::vector<std::pair<std::string,std::wstring>>{
      {"ui-scale",L"0"},{"ui-scale",L"-1"},{"ui-scale",L"nan"},{"ui-scale",L"1.2oops"},
      {"frame-buffer-size",L"0"},{"frame-buffer-size",L"1.2"},{"frame-buffer-size",L"2147483648"},
      {"display-number",L"-1"},{"left-peak-nits",L"0"},{"right-peak-nits",L"10001"},{"boost-tone",L"-1"}}) {
      auto s = fixture(); set(s,c.first,c.second); bad(s,"E_VALUE");
    }
    auto s = fixture(); set(s,"boost-tone",L"0"); set(s,"wheel-sensitivity",L"0"); set(s,"left-peak-nits",L"1"); set(s,"right-peak-nits",L"10000"); good(s);
    s.right_inputs[0].overrides["peak-nits"] = {OverrideMode::Replace,L"10001"}; bad(s,"E_VALUE");
    s=fixture(); set(s,"ui-scale",std::wstring(4000,L'9')); bad(s,"E_VALUE");
  });
  run("validation.explicit-empty-defaults", [] {
    auto s=fixture(); set(s,"aspect-lock",L""); set(s,"aspect-view-mode",L""); set(s,"conversion-fit",L"");
    const auto p=good(s);
    require(option_value(p,L"--aspect-lock").empty() && option_value(p,L"--conversion-fit").empty(),"explicit default clear lost");
    set(s,"display-mode",L""); bad(s,"E_VALUE");
  });
  run("validation.dimensions-and-conflicts", [] {
    for (const auto* value : {L"1280x720",L"1280x",L"x720"}) { auto s=fixture(); set(s,"window-size",value); good(s); }
    for (const auto* value : {L"x",L"0x720",L"1280x0",L"1280x720junk",L"-1x720"}) { auto s=fixture(); set(s,"window-size",value); bad(s,"E_VALUE"); }
    auto s=fixture(); set(s,"window-size",L"1280x"); flag(s,"window-fit-display",true); bad(s,"E_CONFLICT");
    s=fixture(); set(s,"conversion-size",L"x720"); bad(s,"E_VALUE");
    s=fixture(); set(s,"conversion-size",L"max"); good(s);
  });
  run("validation.modes", [] {
    for (const auto* value : {L"split",L"vstack",L"hstack"}) { auto s=fixture(); set(s,"display-mode",value); good(s); }
    for (const auto* value : {L"off",L"on",L"pp"}) { auto s=fixture(); set(s,"auto-loop-mode",value); good(s); }
    for (const auto* value : {L"stretch",L"original",L"dynamic",L"16:9",L"4:3",L"1:1",L"16x9",L"4x3",L"1x1"}) { auto s=fixture(); set(s,"aspect-view-mode",value); good(s); }
    for (const auto* value : {L"auto",L"off",L"on",L"rel"}) { auto s=fixture(); set(s,"tone-map-mode",value); good(s); }
    auto s=fixture(); set(s,"auto-loop-mode",L"repeat"); bad(s,"E_VALUE");
    s=fixture(); s.left_options["color-space"]=L"a:b"; bad(s,"E_PAIR");
  });
  run("validation.time-expressions", [] {
    for (const auto* value : {L"0.150",L"-0.1",L"1:30.5",L"1:02:03.5",L"x1.04+0.1",L"x25.025/24-1:30.5",L"x0.5",L"1e-3"}) {
      auto s=fixture(); set(s,"time-shift",value); require(option_value(good(s),L"--time-shift")==value,"time rewritten");
    }
    for (const auto* value : {L"",L"x",L"x0",L"x1/0",L"x-1",L"x1/2/3",L"1:2:3:4",L"1:30oops",L"1e99",L"nan"}) {
      auto s=fixture(); set(s,"time-shift",value); bad(s,"E_VALUE");
    }
    auto s=fixture(); set(s,"time-shift",L"-1:30.5"); require(diagnostic(build_comparison(s),"W_SIGNED_TIMESTAMP"),"missing signed-component warning");
  });
  run("validation.override-modes", [] {
    auto s=fixture(); s.right_inputs[0].overrides["decoder"]={OverrideMode::Append,L"h264"}; bad(s,"E_APPEND");
    s=fixture(); s.right_inputs[0].overrides["filters"]={OverrideMode::Append,L""}; bad(s,"E_APPEND");
    s=fixture(); s.right_inputs[0].overrides["filters"]={OverrideMode::Replace,L"a::decoder=oops"}; bad(s,"E_RIGHT_SEPARATOR");
  });
  run("inputs.order-enable-copy-stable-ids", [] {
    auto s=fixture(); const auto second=add_right(s,{InputKind::File,L"second.mp4"});
    s.right_inputs[1].overrides["filters"]={OverrideMode::Replace,L"format=gray"};
    const auto duplicate=duplicate_right(s,second); require(duplicate!=second,"duplicate reused id");
    require(move_right(s,second,0),"move failed");
    s.right_inputs[1].enabled=false; s.right_inputs[1].source.text.clear();
    const auto p=good(s);
    require(p.arguments[p.arguments.size()-2]==LR"(D:\fixture\second.mp4::filters=format=gray)","config did not move with input");
    require(std::any_of(p.emissions.begin(),p.emissions.end(),[](const auto& e){return e.field=="right[2].filters";}),"trace lost id");
    require(remove_right(s,duplicate) && !remove_right(s,duplicate),"remove id failed");
    require(!move_right(s,second,99),"bad destination accepted");
  });
  run("inputs.missing-and-invalid-ids", [] {
    auto s=fixture(); s.left.text.clear(); bad(s,"E_INPUT");
    s=fixture(); s.right_inputs[0].enabled=false; bad(s,"E_NO_RIGHT");
    s=fixture(); s.right_inputs.push_back(s.right_inputs[0]); bad(s,"E_INPUT_ID");
    s=fixture(); s.schema_version=2; bad(s,"E_SCHEMA");
  });
  run("inputs.eleven-rights", [] {
    auto s=fixture(); for(int i=0;i<10;++i) add_right(s,{InputKind::File,L"variant "+std::to_wstring(i)+L".mp4"});
    require(good(s).arguments.size()==15,"1+11 argv size");
  });
  run("inputs.reference-resolution", [] {
    auto s=fixture(); s.right_inputs[0].source={InputKind::Reference,L""};
    require(good(s).arguments.back()==LR"(D:\fixture\left.mp4)","self comparison");
    s=fixture(); s.left={InputKind::Reference,L"__"}; s.right_inputs[0].overrides["filters"]={OverrideMode::Replace,L"format=gray"};
    const auto p=good(s); require(p.arguments[p.arguments.size()-2]==LR"(D:\fixture\right.mp4)","left reference included right overrides");
    s.right_inputs[0].source={InputKind::Reference,L"__"}; bad(s,"E_REFERENCE");
  });
  run("inputs.paths-addresses-fonts-and-config", [] {
    auto s=fixture(); s.left.text=LR"(素材\原片 & (一)#.mp4)"; s.right_inputs[0].source={InputKind::Address,L"https://example.invalid/video?q=a&b=c"};
    set(s,"font",LR"(字体\等宽.ttf)"); set(s,"options-file",L"设置.opt");
    const auto p=good(s); require(p.arguments[p.arguments.size()-2]==LR"(D:\fixture\素材\原片 & (一)#.mp4)","Unicode path normalization");
    require(p.arguments.back()==L"https://example.invalid/video?q=a&b=c","URL altered");
    require(option_value(p,L"--font")==LR"(D:\fixture\字体\等宽.ttf)","font path");
    require(option_value(p,L"--options-file")==LR"(D:\fixture\设置.opt)","config path");
    s.right_inputs[0].source.text=L"https://[::1]/v.mp4"; bad(s,"E_RIGHT_SEPARATOR");
  });
  run("inputs.sequence-script-and-dash", [] {
    auto s=fixture(); s.left={InputKind::ImageSequence,L"frame-%04d.png"}; s.right_inputs[0].source={InputKind::Script,L"-script.vpy"};
    auto p=good(s); require(p.arguments[p.arguments.size()-3]==L"--","missing option terminator");
    require(p.arguments.back()==LR"(D:\fixture\-script.vpy)","script path");
    s.left.text=LR"(C:relative.mp4)"; bad(s,"E_PATH_BASE");
  });
  run("validation.invalid-unicode", [] {
    auto s=fixture(); s.left.text=std::wstring(L"left\0.mp4",9); bad(s,"E_INPUT");
    s=fixture(); set(s,"filters",std::wstring(1,static_cast<wchar_t>(0xD800))); bad(s,"E_VALUE");
    s=fixture(); s.engine_path.clear(); bad(s,"E_PATH");
  });
  run("config.sources-order-and-uncertainty", [] {
    auto s=fixture(); set(s,"options-file",L"first.opt"); set(s,"options-file",L"second.opt");
    flag(s,"fullscreen",false); set(s,"window-size",L"1280x720");
    const auto result=build_comparison(s); require(result.ok() && diagnostic(result,"W_EXTERNAL_CONFIG"),"missing external-config uncertainty");
    const auto& a=result.plan->arguments;
    auto first=std::find(a.begin(),a.end(),LR"(D:\fixture\first.opt)"); auto second=std::find(a.begin(),a.end(),LR"(D:\fixture\second.opt)");
    require(first<second,"config file order"); require(!has(*result.plan,L"--fullscreen"),"false emitted");
    flag(s,"disable-auto-options-file",false); require(!has(good(s),L"--no-auto-options-file"),"auto config switch reversed");
    s.right_inputs[0].overrides["filters"]={OverrideMode::Append,L"format=gray"};
    require(good(s).arguments.back()==LR"(D:\fixture\right.mp4::filters=__,format=gray)","unknown template prematurely flattened");
  });
  run("query.isolation-and-empty-search", [] {
    auto s=fixture(); auto q=build_query(s.engine_path,s.working_directory,"find-filters",L"");
    require(q.ok() && q.plan->arguments.back().empty(),"empty search lost");
    require(!build_query(s.engine_path,s.working_directory,"find-filters").ok(),"missing search accepted");
    require(!build_query(s.engine_path,s.working_directory,"help",L"x").ok(),"unexpected query argument");
    require(!build_query(s.engine_path,s.working_directory,"fullscreen").ok(),"comparison flag as query");
  });
  run("preview.trace-control-characters-and-no-mutation", [] {
    auto s=fixture(); set(s,"filters",L"drawtext=text='a\nb'\t");
    const auto a=good(s), b=good(s); require(a.arguments==b.arguments,"non-deterministic build");
    require(a.preview.find(L"\\n")!=std::wstring::npos && a.preview.find(L"\\t")!=std::wstring::npos,"invisible preview characters");
    require(option_value(a,L"--filters")==L"drawtext=text='a\nb'\t","execution value changed");
  });
  run("validation.command-length", [] {
    auto s=fixture(); set(s,"filters",std::wstring(33000,L'a')); bad(s,"E_COMMAND_TOO_LONG");
  });
}

void roundtrip(const std::vector<std::wstring>& expected) {
  const auto command=make_windows_command_line(expected);
  int count=0; auto** parsed=CommandLineToArgvW(command.c_str(),&count);
  require(parsed!=nullptr,"Windows argument parser failed");
  std::vector<std::wstring> actual;
  for(int i=0;i<count;++i) actual.emplace_back(parsed[i]);
  LocalFree(parsed);
  require(actual==expected,"Windows argv roundtrip mismatch");
}
void quoting_tests() {
  run("quoting.Windows-parser-golden", [] {
    roundtrip({LR"(D:\工具 目录\video-compare.exe)",L"",LR"(D:\视频\原片 & (一)#.mp4)",LR"(C:\trailing\)",
               L"a\"b",L"a\\\"b",L"\t\n",L"__",L"::filters=",L"$() ` ^ %PATH% !",L"视频\U0001F3AC.mp4"});
  });
  run("quoting.deterministic-random-roundtrip-1000", [] {
    std::mt19937 random(20260919);
    const std::wstring alphabet=LR"(abcXYZ09 \"'&|^%$()[]{}=:;#中)" L"\t\r\n";
    for(int sample=0;sample<1000;++sample) {
      std::vector<std::wstring> args{LR"(C:\test path\engine.exe)"};
      const auto count=random()%9+1;
      for(unsigned i=0;i<count;++i) {
        std::wstring value; const auto length=random()%100;
        for(unsigned j=0;j<length;++j) value+=alphabet[random()%alphabet.size()];
        args.push_back(value);
      }
      roundtrip(args);
    }
  });
  run("quoting.reject-NUL-and-missing-executable", [] {
    bool threw=false; try { quote_windows_argument(std::wstring(L"a\0b",3)); } catch(const std::invalid_argument&) { threw=true; }
    require(threw,"NUL not rejected"); threw=false;
    try { make_windows_command_line({}); } catch(const std::invalid_argument&) { threw=true; }
    require(threw,"missing argv accepted");
  });
}
}  // namespace

int main(int argc,char** argv) {
  const std::string group=argc>1 ? argv[1] : "all";
  if(group=="catalog" || group=="all") catalog_tests();
  if(group=="overrides" || group=="all") override_tests();
  if(group=="semantics" || group=="all") semantic_tests();
  if(group=="quoting" || group=="all") quoting_tests();
  std::cout << "RESULT passed=" << passed << " failed=" << failed << '\n';
  return failed==0 && passed>0 ? 0 : 1;
}
