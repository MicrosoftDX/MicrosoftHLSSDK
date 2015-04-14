#include "pch.h"
#include "ABI\HLSController.h"
#include "Utilities\ErrorReporter.h"


using namespace Microsoft::DPE::HLSClient;

ErrorReporter::ErrorReporter(CHLSController *controller) : pController(controller)
{
}
void ErrorReporter::ReportError(wstring ErrorMessage,string File, unsigned int LineNum)
{

  if(pController == nullptr || pController->ReportType == ErrorReportType::NONE) return;

  auto seppos = File.find_last_of("\\");
  if(seppos != string::npos)
  {
    File = File.substr(seppos + 1,File.size() - seppos);
  }

  SYSTEMTIME sysTime;
  FILETIME fileTime;
  ::GetSystemTime(&sysTime);
  ::SystemTimeToFileTime(&sysTime,&fileTime);

  ULARGE_INTEGER ts;
  ts.LowPart = fileTime.dwLowDateTime;
  ts.HighPart = fileTime.dwHighDateTime; 

  if(pController->ReportType == ErrorReportType::SYNC)
  {
    pController->RaiseErrorReport((INT64)ts.QuadPart,ErrorMessage,File,LineNum);
  }
  else
  {
    task<void>([this,ts,ErrorMessage,File,LineNum]() { pController->RaiseErrorReport((INT64)ts.QuadPart,ErrorMessage,File,LineNum); });
  }
  
}