/*********************************************************************************************************************
Microsft HLS SDK for Windows

Copyright (c) Microsoft Corporation

All rights reserved.

MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation
files (the ""Software""), to deal in the Software without restriction, including without limitation the rights to use, copy,
modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software
is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

***********************************************************************************************************************/
#pragma once

#define LOGGER_INCL
#include <pch.h>
#include <memory>
#include <ppltasks.h>
#include <sstream>
#include <string> 
#include <windows.storage.h>  
#include <windows.applicationmodel.core.h>
#include <windows.ui.core.h> 

using namespace std;
using namespace Concurrency;


#if defined(_VSLOG) && defined(LOGGER_INCL) && defined(_DEBUG)


#define STREAM(s)\
{\
  wostringstream _log;\
  _log << s;\
}\

#define LOG(s) \
{\
  SYSTEMTIME systime;\
  GetSystemTime(&systime);\
  std::wostringstream _log;\
  _log <<"["<<systime.wHour<<":"<<systime.wMinute<<":"<<systime.wSecond<<":"<<systime.wMilliseconds<<",Thread: "<<GetCurrentThreadId()<<"] "<<s <<"\r\n"; \
  OutputDebugString(_log.str().data());\
}\

#define LOGIF(cond,s) \
{\
if(cond)\
{\
  SYSTEMTIME systime;\
  GetSystemTime(&systime);\
  std::wostringstream _log;\
  _log << "[" << systime.wHour << ":" << systime.wMinute << ":" << systime.wSecond << ":" << systime.wMilliseconds << ",Thread: " << GetCurrentThreadId() << "] " << s << "\r\n"; \
  OutputDebugString(_log.str().data());\
}\
}\

#define LOGIIF(cond,s1,s2) \
{\
if(cond)\
{\
  SYSTEMTIME systime;\
  GetSystemTime(&systime);\
  std::wostringstream _log;\
  _log << "[" << systime.wHour << ":" << systime.wMinute << ":" << systime.wSecond << ":" << systime.wMilliseconds << ",Thread: " << GetCurrentThreadId() << "] " << s1 << "\r\n"; \
  OutputDebugString(_log.str().data());\
}\
  else\
{\
  SYSTEMTIME systime;\
  GetSystemTime(&systime);\
  std::wostringstream _log;\
  _log << "[" << systime.wHour << ":" << systime.wMinute << ":" << systime.wSecond << ":" << systime.wMilliseconds << ",Thread: " << GetCurrentThreadId() << "] " << s2 << "\r\n"; \
  OutputDebugString(_log.str().data());\
}\
}\

#define LOGIFNOT(cond,s) \
{\
if(!(cond))\
{\
  SYSTEMTIME systime;\
  GetSystemTime(&systime);\
  std::wostringstream _log;\
  _log << "[" << systime.wHour << ":" << systime.wMinute << ":" << systime.wSecond << ":" << systime.wMilliseconds << ",Thread: " << GetCurrentThreadId() << "] " << s << "\r\n"; \
  OutputDebugString(_log.str().data());\
}\
}\

#define BINLOG(nm,b,s) 

#elif defined(_FILELOG) && defined(LOGGER_INCL) && defined(_DEBUG)

#define LOG(s) \
{\
  SYSTEMTIME systime;\
  GetSystemTime(&systime);\
  std::wostringstream _log;\
  _log <<"["<<systime.wHour<<":"<<systime.wMinute<<":"<<systime.wSecond<<":"<<systime.wMilliseconds<<"] "<<s <<"\r\n"; \
  FileLogger::Current()->OutputFileLog(_log.str().data());\
}\

#define LOGIF(cond,s) \
{\
if(cond)\
{\
  SYSTEMTIME systime;\
  GetSystemTime(&systime);\
  std::wostringstream _log;\
  _log <<"["<<systime.wHour<<":"<<systime.wMinute<<":"<<systime.wSecond<<":"<<systime.wMilliseconds<<"] "<<s <<"\r\n"; \
  FileLogger::Current()->OutputFileLog(_log.str().data());\
}\
}\

#define LOGIIF(cond,s1,s2) \
{\
if(cond)\
{\
  SYSTEMTIME systime;\
  GetSystemTime(&systime);\
  std::wostringstream _log;\
  _log <<"["<<systime.wHour<<":"<<systime.wMinute<<":"<<systime.wSecond<<":"<<systime.wMilliseconds<<"] "<<s1 <<"\r\n"; \
  FileLogger::Current()->OutputFileLog(_log.str().data());\
}\
  else\
{\
  SYSTEMTIME systime;\
  GetSystemTime(&systime);\
  std::wostringstream _log;\
  _log <<"["<<systime.wHour<<":"<<systime.wMinute<<":"<<systime.wSecond<<":"<<systime.wMilliseconds<<"] "<<s2 <<"\r\n"; \
  FileLogger::Current()->OutputFileLog(_log.str().data());\
}\
}\
#define LOGIFNOT(cond,s) \
{\
if(!(cond))\
{\
  SYSTEMTIME systime;\
  GetSystemTime(&systime);\
  std::wostringstream _log;\
  _log <<"["<<systime.wHour<<":"<<systime.wMinute<<":"<<systime.wSecond<<":"<<systime.wMilliseconds<<"] "<<s <<"\r\n"; \
  FileLogger::Current()->OutputFileLog(_log.str().data());\
}\
}\
 

#define BINLOG(nm,b,s) 

#elif defined(_BINLOG) && defined(LOGGER_INCL) && defined(_DEBUG)


#define BINLOG(nm,b,s) \
{\
  std::wostringstream _log;\
  _log << nm;\
  FileLogger::Current()->OutputFullFile(_log.str().data(),b,s);\
}\

#define LOG(s) 
#define LOGIF(cond,s)
#define LOGIIF(cond,s1,s2)
#define LOGIFNOT(cond,s) 

#else

#define LOG(s) 
#define LOGIF(cond,s)
#define LOGIIF(cond,s1,s2)
#define LOGIFNOT(cond,s)
#define BINLOG(nm,b,s) 

#endif

namespace Microsoft {
  namespace HLSClient {
    namespace Private {


      class FileLogger
      {
      public:

        FileLogger()
        {

          SYSTEMTIME systime;
          ::GetSystemTime(&systime);
          std::wostringstream fnm;
          fnm << L"mshls_" << systime.wYear << "_" << systime.wMonth << "_" << systime.wDay << "_" << systime.wHour << "_" << systime.wMinute << "_" << systime.wSecond << ".log";



          Concurrency::create_task(Windows::Storage::KnownFolders::VideosLibrary->CreateFileAsync(ref new Platform::String(fnm.str().data()),
            Windows::Storage::CreationCollisionOption::GenerateUniqueName)).get();

          return;


        }


      public:
        Windows::Storage::IStorageFile^ pFile;
        static shared_ptr<FileLogger> pFileLogger;
        Windows::UI::Core::ICoreDispatcher^ cordisp;
        static shared_ptr<FileLogger> Current()
        {
          if (pFileLogger == nullptr)
            pFileLogger = std::make_shared<FileLogger>();
          return pFileLogger;
        }

        void OutputFileLog(const WCHAR *Message)
        {

          std::wstring line = Message;
          line.append(L"\r\n");
          Windows::Storage::FileIO::AppendTextAsync(pFile, ref new Platform::String(line.data()));

        }

        void OutputBinLog(BYTE *Message, unsigned int size)
        {
          Platform::Array<BYTE>^ arr = ref new Platform::Array<BYTE>(size);
          for (unsigned int i = 0; i < size; i++)
            arr->set(i, Message[i]);
          Windows::Storage::FileIO::WriteBytesAsync(pFile, arr);


        }

        void OutputFullFile(wstring FileName, BYTE *Message, unsigned int size)
        {
          HRESULT hr = S_OK;

          Windows::Storage::IStorageFile^ pFile;
          Concurrency::create_task(Windows::Storage::KnownFolders::VideosLibrary->CreateFileAsync(ref new Platform::String(FileName.data()),
            Windows::Storage::CreationCollisionOption::GenerateUniqueName)).then([this, Message, size](Windows::Storage::IStorageFile^ fl)
          {
            Platform::Array<BYTE>^ arr = ref new Platform::Array<BYTE>(size);
            for (unsigned int i = 0; i < size; i++)
              arr->set(i, Message[i]);
            Concurrency::create_task(Windows::Storage::FileIO::WriteBytesAsync(fl, arr));
          });

        }

      };

    }
  }
} 