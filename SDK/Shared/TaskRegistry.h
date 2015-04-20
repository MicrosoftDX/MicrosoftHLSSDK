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
#include <ppltasks.h>
#include <map>
#include <vector>
#include <typeinfo>
#include <wtypes.h>
#include <string>
#include <memory> 
#include <mutex>
#include "FileLogger.h"


//#define CHKTASK() \
//  if (Concurrency::is_task_cancellation_requested()) \
//  {  \
//    Concurrency::cancel_current_task(); \
//  }\


#define CHKTASK(curtok) \
  if (curtok.is_canceled()) \
  {  \
    Concurrency::cancel_current_task(); \
  }\

using namespace std;
using namespace concurrency;

namespace Microsoft {
  namespace HLSClient {
    namespace Private {



      template<typename TaskReturnType>
      class TaskRegistry
      {
      private:
        recursive_mutex LockAccess;

        std::vector<task<TaskReturnType>> vecTasks;
        std::vector<Concurrency::cancellation_token_source> _runningbatch;

       

      public:
        TaskRegistry()
        {

        }

        static task<void> NoOpTask()
        {
          return task<void>([]()
          {
            int i = 1;
            ++i;
            return;
          });
        }
        int Count()
        {
          lock_guard<recursive_mutex> lock(LockAccess);

          return vecTasks.size();
        }

        void CleanupCompletedOrCanceled()
        {
          try
          {
            if (vecTasks.size() == 0)
              return;

            auto done = std::find_if(vecTasks.begin(), vecTasks.end(), [this](task<TaskReturnType> t)
            {
              return t.is_done();
            });
            while (done != vecTasks.end() && vecTasks.size() > 0)
            {
              vecTasks.erase(done);
              done = std::find_if(vecTasks.begin(), vecTasks.end(), [this](task<TaskReturnType> t)
              {
                return t.is_done();
              });
            }

           
          }
          catch (...)
          {

          }
        }


        void CancelAll(bool WaitForCancelComplete = false)
        {
          lock_guard<recursive_mutex> lock(LockAccess);

          for (auto itm : _runningbatch)
            itm.cancel();

          if (WaitForCancelComplete) {
        
            if (vecTasks.size() > 0)
              when_all(vecTasks.begin(), vecTasks.end()).wait();

            CleanupCompletedOrCanceled();
          }

          _runningbatch.clear();
        }

        void Register(task<TaskReturnType> t, Concurrency::cancellation_token_source tokensrc)
        {
          lock_guard<recursive_mutex> lock(LockAccess);
        

          _runningbatch.push_back(tokensrc);
          vecTasks.push_back(t);

          CleanupCompletedOrCanceled();
        }

        void Register(task<TaskReturnType> t)
        {
          lock_guard<recursive_mutex> lock(LockAccess);
          if (t.is_done() == false)
          {
            //CleanupCompletedOrCanceled();
            vecTasks.push_back(t);
          }
        }
 

        task<void> WhenAll()
        {
          unique_lock<recursive_mutex> lock(LockAccess, try_to_lock);
          if (lock.owns_lock())
          {
            CleanupCompletedOrCanceled();
            if (vecTasks.size() > 0)
            {
              return when_all(vecTasks.begin(), vecTasks.end()).then([](task<vector<TaskReturnType>> tret) { return NoOpTask(); });
            }
            else
              return NoOpTask();// task<void>([]() { return; });

          }
          else
            return NoOpTask();// task<void>([]() { return; });
        }
		        
		

      };
    }
  }
} 