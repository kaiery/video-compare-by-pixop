#include "runtime.h"
int wmain(int argc,wchar_t** argv) {
  if(argc!=3||std::wstring(argv[1])!=L"--request")return 2;
  return launcher::process::run_request(argv[2]);
}
