#pragma once
#include "model.h"
namespace launcher::storage {
std::string read_bytes(const std::wstring& path);
void write_bytes(const std::wstring& path,const std::string& bytes);
std::wstring decode(const std::string& bytes);
std::string encode(const std::wstring& text);
void save_session(const std::wstring& path,const Session& session);
Session load_session(const std::wstring& path);
// Exact engine tokenization, including whole-remainder # comments and lost empty tokens.
std::vector<std::wstring> tokenize(const std::wstring& text);
std::wstring serialize_options(const std::vector<std::wstring>& arguments);
struct Import { Session session; bool editable{}; std::wstring report; };
Import import_options(const std::wstring& path,const Session& current);
void export_options(const std::wstring& path,const Session& session);
std::wstring inspect_sources(const Session& session);
std::wstring compatibility(const std::wstring& help);
LaunchPlan prepare_launch(LaunchPlan plan,const std::wstring& cache_root);
}
