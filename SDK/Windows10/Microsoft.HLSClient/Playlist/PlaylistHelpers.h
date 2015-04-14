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

 

#define ELEM_AUDIO_SAMPLE_LEN 1024

#include "pch.h"
#include <sstream>   
#include <string>
#include <algorithm>
#include <vector>
#include <cmath>
#include <wtypes.h>

using namespace std;

namespace Microsoft {
  namespace HLSClient {
    namespace Private {



      struct TAGNAME
      {
      public:
        static const wchar_t *TAGBEGIN;
        static const wchar_t *EXTM3U;
        static const wchar_t *EXTINF;
        static const wchar_t *EXT_X_BYTERANGE;
        static const wchar_t *EXT_X_TARGETDURATION;
        static const wchar_t *EXT_X_MEDIA_SEQUENCE;
        static const wchar_t *EXT_X_KEY;
        static const wchar_t *EXT_X_PROGRAM_DATE_TIME;
        static const wchar_t *EXT_X_ALLOW_CACHE;
        static const wchar_t *EXT_X_PLAYLIST_TYPE;
        static const wchar_t *EXT_X_ENDLIST;
        static const wchar_t *EXT_X_MEDIA;
        static const wchar_t *EXT_X_STREAM_INF;
        static const wchar_t *EXT_X_DISCONTINUITY;
        static const wchar_t *EXT_X_I_FRAMES_ONLY;
        static const wchar_t *EXT_X_I_FRAMES_STREAM_INF;
        static const wchar_t *EXT_X_VERSION;
      };



      struct Helpers
      {

        
        static bool ToProgramDateTime(std::wstring szdt, unsigned long long& pdt)
        {
          //validation checks
          if (szdt.empty()) return false;
          size_t TPos = szdt.find(L'T');
          wstring datepart, timepart;
          SYSTEMTIME systime;
          ZeroMemory(&systime,sizeof(systime));


          unsigned int YYYY = 0;
          unsigned short YY = 0;
          unsigned short CC = 0;
          unsigned int MM = 0;
          unsigned int DD = 0;
          unsigned int hh = 0;
          unsigned int mm = 0;
          int tzbiasmins = 0; 
          double ss = 0.0;
          if (TPos != wstring::npos)// 
          {
            datepart = szdt.substr(0, TPos);
            timepart = szdt.substr(TPos + 1, szdt.size() - TPos - 1);
          }
          else
            datepart = szdt;

          if (std::count(datepart.begin(), datepart.end(), L'-') < 2 || datepart.size() != 10 || (timepart.empty() == false && std::count(timepart.begin(), timepart.end(), L':') < 2))
            return false;


          wistringstream(datepart.substr(0, 4)) >> systime.wYear;
          wistringstream(datepart.substr(2, 2)) >> YY;
          wistringstream(datepart.substr(0, 2)) >> CC;
          wistringstream(datepart.substr(5, 2)) >> systime.wMonth;
          wistringstream(datepart.substr(8, 2)) >> systime.wDay;


          //zeller's algorithm for DOW
          // [dom + [(13 * MMShifted - 1)/5] + YY + (YY/4) + (CC/4) - (2*CC)] % 7

          systime.wDayOfWeek = (unsigned short) (systime.wDay + floor((double) ((13 * (systime.wMonth <= 2 ? systime.wMonth + 10 : systime.wMonth - 2)) - 1) / 5) + YY + floor((double) YY / 4) + floor((double) CC / 4) - (2 * CC)) % 7;

          if (timepart.empty() == false)
          {
            wistringstream(timepart.substr(0, 2)) >> systime.wHour;
            wistringstream(timepart.substr(3, 2)) >> systime.wMinute;

            int biastype = 1;
            size_t tzpos = timepart.find(L'+');
            if (tzpos == wstring::npos)
            {
              tzpos = timepart.find(L'-');
              if (tzpos != wstring::npos)
                biastype = -1;
            }
                
            wstring secpart = (tzpos != wstring::npos) ? timepart.substr(6, tzpos - 6) : timepart.substr(6, timepart.size() - 6);
            size_t decpos = secpart.find(L'.');
            if (decpos != wstring::npos)
            {
              wistringstream(secpart.substr(0,decpos)) >> systime.wSecond;
              double fractionsecs = 0.0;
              wistringstream(secpart.substr(decpos, secpart.size() - decpos)) >> fractionsecs;
              systime.wMilliseconds = (WORD)(fractionsecs * 1000);
            }
            else
            {
              wistringstream(secpart) >> systime.wSecond;
              systime.wMilliseconds = 0;
            }

            if (tzpos != wstring::npos)
            {
              wstring tz = timepart.substr(tzpos + 1, timepart.size() - tzpos - 1);
              if (tz.find(L':'))
              {
                unsigned int tzh = 0;
                unsigned int tzm = 0;
                wistringstream(tz.substr(0, 2)) >> tzh;
                wistringstream(tz.substr(3, 2)) >> tzm;
                tzbiasmins = biastype * ((tzh * 60) + tzm);
              }
            }
           
          }

          FILETIME ftm;
          ZeroMemory(&ftm, sizeof(FILETIME));
          SystemTimeToFileTime(&systime, &ftm);
          ULARGE_INTEGER uiftm;
          uiftm.HighPart = ftm.dwHighDateTime;
          uiftm.LowPart = ftm.dwLowDateTime;

          if (tzbiasmins != 0)
            pdt = uiftm.QuadPart + (tzbiasmins * 60 * 10000000);
          else
            pdt = uiftm.QuadPart;

          return true;
           
        }
        ///<summary>Checks to see if an URL is absolute or not</summary>
        static bool IsAbsoluteUri(const std::wstring& Uri)
        {
          std::wstring temp = Uri;
          auto upper = Helpers::ToUpper(temp);
          return !(upper.find(L"HTTP://") == std::wstring::npos && upper.find(L"HTTPS://") == std::wstring::npos); //no scheme component - assume relative
        }

        ///<summary>Splits a URL into the resource name and the rest of the URL </summary>
        static bool SplitUri(const std::wstring& Uri, std::wstring& Base, std::wstring& FileName)
        {
          std::wstring::size_type lastforwardslashpos = 0;
          //does this have a query string ?
          auto queryseppos = Uri.find_last_of('?');
          if (queryseppos == wstring::npos)
          {
            lastforwardslashpos = Uri.find_last_of('/');
          }
          else
          {
            lastforwardslashpos = Uri.find_last_of(L"/", queryseppos - 1);
          }
           
          FileName = Uri.substr(lastforwardslashpos + 1, Uri.size() - (lastforwardslashpos + 1));
          Base = Uri.substr(0, lastforwardslashpos + 1);
          return !Base.empty() && !FileName.empty();
        }

        ///<summary>Joins a base URI and resource name into a valid URL</summary>
        static std::wstring JoinUri(const std::wstring& Base, const std::wstring& FileName)
        {
          std::wstring temp = L"";
          if (Base.back() == '/' && FileName.front() == '/')
            temp = Base.substr(0, Base.size() - 1) + FileName;
          else if (Base.back() != '/' && FileName.front() != '/')
            temp = Base + L"/" + FileName;
          else
            temp = Base + FileName;

          return temp;
        }

        ///<summary>Removes carriage returns from a string</summary>
        static std::wstring& RemoveCarriageReturn(std::wstring& target)
        {
          if (target.empty()) return target;
          auto newend = std::remove(target.begin(), target.end(), '\r');
          if (newend != target.end()) target.erase(newend, target.end());
          return target;
        }

        ///<summary>Trims leading and trailing white spaces from a string</summary>
        static std::wstring& Trim(std::wstring& target)
        {
          if (target.empty()) return target;
          std::wstring::iterator itr;
          for (itr = target.begin(); itr != target.end(); itr++)
          {
            if (*itr != ' ' && *itr != '\t')
              break;
          }
          if (itr != target.begin()) target.erase(target.begin(), itr);

          if (target.size() > 0)
          {
            std::wstring::iterator ritr;
            for (ritr = target.end() - 1; ritr != target.begin(); ritr--)
            {
              if (*ritr != ' ' && *ritr != '\t')
                break;
            }

            if (ritr != target.begin()) target.erase(ritr, target.end() - 1);
          }

          return target;
        }

        ///<summary>Transforms a string to uppercase</summary>
        static std::wstring ToUpper(std::wstring& target)
        {
          std::wstring temp = target;
          if (target.empty()) return temp;
          std::transform(begin(temp), end(temp), begin(temp), ::towupper);
          return temp;
        }

        ///<summary>Read the list of attribute name/value pairs from an HLS tag string</summary>
        static std::wstring ReadAttributeList(const std::wstring& tagline)
        {
          auto colonpos = tagline.find_first_of(':');
          if (colonpos == std::wstring::npos)
            return std::wstring(L"");
          else
            return tagline.substr(colonpos + 1, tagline.size() - colonpos);
        }

        ///<summary>Extracts the tag name from a tag string</summary>
        static std::wstring ReadTagName(const std::wstring& tagline)
        {
          if (tagline[0] != '#') return L"";
          std::wstring tagName;
          auto posColon = tagline.find_first_of(':');
          if (posColon != std::wstring::npos)
            tagName = tagline.substr(1, posColon - 1);
          else
            tagName = tagline.substr(1, tagline.size() - 1);

          return ToUpper(tagName);
        }

        ///<summary>Splits an HLS tag attribute list into a list of name/value strings</summary>
        ///<param name='sep'>The separator to base the splitting on</param>
        static std::vector<std::wstring> SplitAttributeList(const std::wstring& allattribs, const wchar_t sep = ',')
        {
          std::vector<std::wstring> vec;
          std::wstring::size_type beginpos = 0;
          auto seppos = allattribs.find(sep, beginpos);

          while (seppos != std::wstring::npos)
          {
            vec.push_back(allattribs.substr(beginpos, seppos - beginpos));
            if (seppos == allattribs.size() - 1)
            {
              beginpos = std::wstring::npos;
              break;
            }
            beginpos = seppos + 1;
            seppos = allattribs.find(sep, beginpos);
          }
          if (beginpos != std::wstring::npos)
            vec.push_back(allattribs.substr(beginpos, allattribs.size() - beginpos));

          return vec;
        }

        ///<summary>Reads an attribute value from a specific index as an unsigned integer/summary>
        ///<param name='allattribs'>Attribute list</param>
        ///<param name='position'>Index of the attribute in the attribute list</param>
        ///<param name='AttribVal'>The returned value</param>
        ///<param name='sep'>Separator used to split the attribute list</param>
        ///<returns>True if read was successful, false otherwise</returns>
        template<typename T>
        static bool ReadAttributeValueFromPosition(const std::wstring& allattribs, unsigned short position, T& AttribVal, const wchar_t sep = ',')
        {

          auto splits = SplitAttributeList(allattribs, sep);
          if (position > splits.size() - 1) return false;
          wistringstream(splits.at(position)) >> AttribVal;
          return true;
        }
        //explicit specialization for wstring
        template<>
        static bool ReadAttributeValueFromPosition(const std::wstring& allattribs, unsigned short position, wstring& AttribVal, const wchar_t sep)
        {

          auto splits = SplitAttributeList(allattribs, sep);
          if (position > splits.size() - 1) return false;
          AttribVal = splits.at(position);
          return true;
        }

        /*
        ///<summary>Reads an attribute value from a specific index as an unsigned integer/summary>
        ///<param name='allattribs'>Attribute list</param>
        ///<param name='position'>Index of the attribute in the attribute list</param>
        ///<param name='AttribVal'>The returned value</param>
        ///<param name='sep'>Separator used to split the attribute list</param>
        ///<returns>True if read was successful, false otherwise</returns>
        static bool ReadAttributeValueFromPosition(const std::wstring& allattribs, unsigned short position,unsigned int& AttribVal,const wchar_t sep = ',')
        {

        auto splits = SplitAttributeList(allattribs,sep);
        if(position > splits.size() - 1) return false;
        wistringstream(splits.at(position)) >> AttribVal;
        return true;
        }

        ///<summary>Reads an attribute value from a specific index as an unsigned long long/summary>
        ///<param name='allattribs'>Attribute list</param>
        ///<param name='position'>Index of the attribute in the attribute list</param>
        ///<param name='AttribVal'>The returned value</param>
        ///<param name='sep'>Separator used to split the attribute list</param>
        ///<returns>True if read was successful, false otherwise</returns>
        static bool ReadAttributeValueFromPosition(const std::wstring& allattribs, unsigned short position,unsigned long long& AttribVal,const wchar_t sep = ',')
        {

        auto splits = SplitAttributeList(allattribs,sep);
        if(position > splits.size() - 1) return false;
        wistringstream(splits.at(position)) >> AttribVal;
        return true;
        }

        ///<summary>Reads an attribute value from a specific index as a double/summary>
        ///<param name='allattribs'>Attribute list</param>
        ///<param name='position'>Index of the attribute in the attribute list</param>
        ///<param name='AttribVal'>The returned value</param>
        ///<param name='sep'>Separator used to split the attribute list</param>
        ///<returns>True if read was successful, false otherwise</returns>
        static bool ReadAttributeValueFromPosition(const std::wstring& allattribs, unsigned short position,double& AttribVal,const wchar_t sep = ',')
        {

        auto splits = SplitAttributeList(allattribs,sep);
        if(position > splits.size() - 1) return false;
        wistringstream(splits.at(position)) >> AttribVal;
        return true;
        }

        ///<summary>Reads an attribute value from a specific index as a string/summary>
        ///<param name='allattribs'>Attribute list</param>
        ///<param name='position'>Index of the attribute in the attribute list</param>
        ///<param name='AttribVal'>The returned value</param>
        ///<param name='sep'>Separator used to split the attribute list</param>
        ///<returns>True if read was successful, false otherwise</returns>
        static bool ReadAttributeValueFromPosition(const std::wstring& allattribs, unsigned short position,std::wstring& AttribVal,const wchar_t sep = ',')
        {

        auto splits = SplitAttributeList(allattribs,sep);
        if(position > splits.size() - 1) return false;
        AttribVal = splits.at(position);
        return true;
        }

        ///<summary>Reads an attribute value from a specific index as a boolean/summary>
        ///<param name='allattribs'>Attribute list</param>
        ///<param name='position'>Index of the attribute in the attribute list</param>
        ///<param name='AttribVal'>The returned value</param>
        ///<param name='sep'>Separator used to split the attribute list</param>
        ///<returns>True if read was successful, false otherwise</returns>
        static bool ReadAttributeValueFromPosition(const std::wstring& allattribs, unsigned short position,bool& AttribVal,const wchar_t sep = ',')
        {

        auto splits = SplitAttributeList(allattribs,sep);
        if(position > splits.size() - 1) return false;
        wistringstream(splits.at(position)) >> AttribVal;
        return true;
        }
        */
        ///<summary>Reads an attribute value for a specific attribute name as an unsigned integer/summary>
        ///<param name='allattribs'>Attribute list</param>
        ///<param name='AttribName'>Attribute name</param>
        ///<param name='AttribVal'>The returned value</param> 
        ///<returns>True if read was successful, false otherwise</returns>
        static bool ReadNamedAttributeValue(const std::wstring& allattribs, const std::wstring& AttribName, unsigned int& AttribVal)
        {

          auto attribpos = allattribs.find(AttribName);
          if (attribpos == std::wstring::npos) return false;
          //TODO: Error check - mandatory attribute has to exist
          auto equalsignpos = allattribs.find_first_of('=', attribpos);
          if (equalsignpos == std::wstring::npos) return false;
          //TODO : error check for not found  
          wistringstream(allattribs.substr(equalsignpos + 1, allattribs.find_first_of(',', equalsignpos) - equalsignpos - 1)) >> AttribVal;
          //TODO:: error check on previous line
          return true;
        }

        ///<summary>Reads an attribute value for a specific attribute name as a boolean/summary>
        ///<param name='allattribs'>Attribute list</param>
        ///<param name='AttribName'>Attribute name</param>
        ///<param name='AttribVal'>The returned value</param> 
        ///<returns>True if read was successful, false otherwise</returns>
        static bool ReadNamedAttributeValue(const std::wstring& allattribs, const std::wstring& AttribName, bool& AttribVal)
        {

          auto attribpos = allattribs.find(AttribName);
          if (attribpos == std::wstring::npos) return false;
          //TODO: Error check - mandatory attribute has to exist
          auto equalsignpos = allattribs.find_first_of('=', attribpos);
          if (equalsignpos == std::wstring::npos) return false;
          std::wstring val = allattribs.substr(equalsignpos + 1, allattribs.find_first_of(',', equalsignpos) - equalsignpos - 1);
          AttribVal = (val == L"NO" ? false : true);
          //TODO:: error check on previous line
          return true;
        }

        ///<summary>Reads an attribute value for a specific attribute name as a string/summary>
        ///<param name='allattribs'>Attribute list</param>
        ///<param name='AttribName'>Attribute name</param>
        ///<param name='AttribVal'>The returned value</param> 
        ///<returns>True if read was successful, false otherwise</returns>
        static bool ReadNamedAttributeValue(const std::wstring& allattribs, const std::wstring& AttribName, std::wstring& AttribVal)
        {
          auto attribpos = allattribs.find(AttribName);
          if (attribpos == std::wstring::npos) return false;
          auto equalpos = allattribs.find_first_of('=', attribpos);
          AttribVal = allattribs[equalpos + 1] != '\"' ? allattribs.substr(equalpos + 1, allattribs.find_first_of(',', equalpos) - equalpos - 1) : allattribs.substr(equalpos + 2, allattribs.find_first_of('\"', equalpos + 2) - equalpos - 2);

          return true;

        }

        ///<summary>Reads attribute values for a specific attribute name as a list of string values/summary>
        ///<param name='allattribs'>Attribute list</param>
        ///<param name='AttribName'>Attribute name</param>
        ///<param name='sep'>Separator used to split the value list</param>
        ///<param name='AttribVal'>The returned list of values</param> 
        ///<returns>True if read was successful, false otherwise</returns>
        static bool ReadNamedAttributeValue(const std::wstring& allattribs, const std::wstring& AttribName, const wchar_t sep, std::vector<std::wstring>& AttribVal)
        {
          auto attribpos = allattribs.find(AttribName);
          if (attribpos == std::wstring::npos) return false;
          //TODO: Error check - mandatory attribute has to exist
          auto startquotepos = allattribs.find_first_of('\"', attribpos);
          if (startquotepos == std::wstring::npos) return false;
          //TODO : error check for not found  
          std::wstring val = allattribs.substr(startquotepos + 1, allattribs.find_first_of('\"', startquotepos) - startquotepos - 1);
          size_t seppos = val.find_first_of(sep, 0);
          size_t prevpos = 0;
          while (seppos > 0 && seppos != std::wstring::npos)
          {
            AttribVal.push_back(val.substr(prevpos, seppos - prevpos));
            prevpos = seppos + 1;
            seppos = val.find_first_of(sep, prevpos);
          }
          return true;
          //TODO:: error check on previous line
        }

        ///<summary>Reads attribute values for a specific attribute name as two unsigned integers separated by an 'X'/summary>
        ///<param name='allattribs'>Attribute list</param>
        ///<param name='AttribName'>Attribute name</param> 
        ///<param name='AttribVal'>The returned list of values</param> 
        ///<returns>True if read was successful, false otherwise</returns>
        static bool ReadNamedAttributeValue(const std::wstring& allattribs, const std::wstring& AttribName, unsigned int& AttribVal1, unsigned int& AttribVal2)
        {
          auto allattribsuc = Helpers::ToUpper(const_cast<std::wstring&>(allattribs));
          auto attribpos = allattribsuc.find(AttribName);
          if (attribpos == std::wstring::npos) return false;
          auto equalsignpos = allattribsuc.find_first_of('=', attribpos);
          if (equalsignpos == std::wstring::npos) return false;
          auto resseparatorpos = allattribsuc.find_first_of('X', equalsignpos);
          if (resseparatorpos == std::wstring::npos) return false;
          //TODO : error check for not found  
          wistringstream(allattribsuc.substr(equalsignpos + 1, resseparatorpos - equalsignpos - 1)) >> AttribVal1;
          wistringstream(allattribsuc.substr(resseparatorpos + 1, allattribs.find_first_of(',', resseparatorpos) - resseparatorpos - 1)) >> AttribVal2;
          return true;
        }

      };
    }
  }
} 