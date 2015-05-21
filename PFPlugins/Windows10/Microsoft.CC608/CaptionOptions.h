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