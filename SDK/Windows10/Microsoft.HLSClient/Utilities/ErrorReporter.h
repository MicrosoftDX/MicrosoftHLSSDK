#pragma once
#include <memory>
#include <sstream>
#include <string>
#include <hstring.h>
#include <functional>
#include <wrl.h>
#include <wrl/wrappers/corewrappers.h> 
#include <windows.foundation.h> 

using namespace std;
using namespace ABI::Windows::Foundation;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;



#define ERR(Reporter,ErrorMessage,File,Line)\
{\
  if(Reporter != nullptr && Reporter->pController != nullptr && Reporter->pController->pMediaSource != nullptr && Reporter->pController->pMediaSource->GetCurrentState() != MediaSourceState::MSS_UNINITIALIZED && Reporter->pController->pMediaSource->GetCurrentState() != MediaSourceState::MSS_ERROR) \
{\
  wostringstream _log;\
  _log << ErrorMessage;\
  Reporter->ReportError(_log.str(),string(File),Line);\
}\
}\


#define ERRIF(Reporter,IfTrue,ErrorMessage,File,Line)\
{\
  if(IfTrue == true && Reporter != nullptr && Reporter->pController != nullptr && Reporter->pController->pMediaSource != nullptr && Reporter->pController->pMediaSource->GetCurrentState() != MediaSourceState::MSS_UNINITIALIZED && Reporter->pController->pMediaSource->GetCurrentState() != MediaSourceState::MSS_ERROR)\
{\
  wostringstream _log;\
  _log << ErrorMessage;\
  Reporter->ReportError(_log.str(),string(File),Line);\
}\
}\

namespace Microsoft { namespace DPE { namespace HLSClient {

  class CHLSController; 
  class ErrorReporter
  {
  private:

  public:
    CHLSController *pController;
    ErrorReporter(CHLSController *controller);
    void ReportError(wstring ErrorMessage,string File, unsigned int LineNum);
  };
}}}

