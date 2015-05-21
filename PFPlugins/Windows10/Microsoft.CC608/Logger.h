#pragma once

#define NOMINMAX
#include <Windows.h>
#include <sstream>

#include "pch.h"

namespace Microsoft { namespace CC608 {

#ifdef _DEBUG

  #define DebugWrite(s) \
  {\
    SYSTEMTIME systime;\
    GetSystemTime(&systime);\
    std::wostringstream _log;\
    _log <<"["<<systime.wHour<<":"<<systime.wMinute<<":"<<systime.wSecond<<":"<<systime.wMilliseconds<<"]608 Captions>"<<s <<"\r\n"; \
    OutputDebugString(_log.str().data());\
  }

#else

  #define DebugWrite(s) ((void)0)

#endif

}}
