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
#include "pch.h"
#include "Playlist\PlaylistHelpers.h"

using namespace Microsoft::HLSClient::Private;

const wchar_t *TAGNAME::TAGBEGIN = L"#EXT";
const wchar_t *TAGNAME::EXTM3U = L"EXTM3U";
const wchar_t *TAGNAME::EXTINF = L"EXTINF";
const wchar_t *TAGNAME::EXT_X_BYTERANGE = L"EXT-X-BYTERANGE";
const wchar_t *TAGNAME::EXT_X_TARGETDURATION = L"EXT-X-TARGETDURATION";
const wchar_t *TAGNAME::EXT_X_MEDIA_SEQUENCE = L"EXT-X-MEDIA-SEQUENCE";
const wchar_t *TAGNAME::EXT_X_KEY = L"EXT-X-KEY";
const wchar_t *TAGNAME::EXT_X_PROGRAM_DATE_TIME = L"EXT-X-PROGRAM-DATE-TIME";
const wchar_t *TAGNAME::EXT_X_ALLOW_CACHE = L"EXT-X-ALLOW-CACHE";
const wchar_t *TAGNAME::EXT_X_PLAYLIST_TYPE = L"EXT-X-PLAYLIST-TYPE";
const wchar_t *TAGNAME::EXT_X_ENDLIST = L"EXT-X-ENDLIST";
const wchar_t *TAGNAME::EXT_X_MEDIA = L"EXT-X-MEDIA";
const wchar_t *TAGNAME::EXT_X_STREAM_INF = L"EXT-X-STREAM-INF";
const wchar_t *TAGNAME::EXT_X_DISCONTINUITY = L"EXT-X-DISCONTINUITY";
const wchar_t *TAGNAME::EXT_X_I_FRAMES_ONLY = L"EXT-X-I-FRAMES-ONLY";
const wchar_t *TAGNAME::EXT_X_I_FRAMES_STREAM_INF = L"EXT-X-I-FRAMES-STREAM-INF";
const wchar_t *TAGNAME::EXT_X_VERSION = L"EXT-X-VERSION";
