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

namespace Microsoft
{
	namespace CC608 
	{
		using namespace Platform;
		using namespace Windows::UI::Xaml::Media;

		/// <summary>Caption options to override values in 608 data</summary>
		[Windows::Foundation::Metadata::WebHostHidden]
		public ref class CaptionOptions sealed
		{
		public:
			/// <summary>Default constructor</summary>
			CaptionOptions();

			/// <summary>the font family</summary>
			property String^ FontFamily;

			/// <summary>the font brush</summary>
			property Brush^ FontBrush;

			/// <summary>the background brush</summary>
			property Brush^ BackgroundBrush;

			/// <summary>the window brush</summary>
			property Brush^ WindowBrush;

			/// <summary>the caption width for proportional-width fonts</summary>
			property double CaptionWidth;

			/// <summary>the video page width</summary>
			property double VideoWidth;

			/// <summary>the font size as a percentage (50, 100, 150, or 200)</summary>
			property int FontSize;

			/// <summary>is the text small caps?</summary>
			property bool IsSmallCaps;
		};
	}
}