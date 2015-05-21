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
