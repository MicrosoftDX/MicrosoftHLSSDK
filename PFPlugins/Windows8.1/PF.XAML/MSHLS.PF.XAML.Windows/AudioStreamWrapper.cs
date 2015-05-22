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
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.PlayerFramework.Adaptive.HLS
{
  /// <summary>
  /// Wraps the MMPPF AudioStream class to represent an internally identifyable audio stream. 
  /// (Used to initially populate the audio tracks--since only this project can instantiate these,
  ///  if we find them later, we know that the app developer did not change them.)
  /// </summary>
  public sealed class AudioStreamWrapper : AudioStream
  {
    // used to identify the audio track internally
    internal string NamePlusLanguageId
    {
      get
      {
        return GetNamePlusLanguageForId(this.Name, this.Language);
      }
    }

    // prevent users from outside the project from constructing instances so that we can be sure 
    // that, if we find one, we know we put it there
    internal AudioStreamWrapper(string name, string language)
      : base(name, language)
    {
    }

    internal static string GetNamePlusLanguageForId(string name, string language)
    {
      return name + "+" + language;
    }
  }
}
