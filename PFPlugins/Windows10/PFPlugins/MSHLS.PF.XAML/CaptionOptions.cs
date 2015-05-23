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
// <copyright file="CaptionOption.cs" company="Microsoft Corporation">
// Copyright (c) 2014 Microsoft Corporation All Rights Reserved
// </copyright>
// <author>Michael S. Scherotter</author>
// <email>mischero@microsoft.com</email>
// <date>2014-01-27</date>
// <summary>Caption Options</summary>

namespace Microsoft.PlayerFramework.CC608
{
    using Windows.UI;
    using Windows.UI.Xaml.Media;

    /// <summary>
    /// Caption options
    /// </summary>
    public class CaptionOptions
    {
        /// <summary>
        /// Initializes a new instance of the CaptionOptions class.
        /// </summary>
        public CaptionOptions()
        {
            //CaptionWidth = 200d;
            this.IsSmallCaps = false;
            this.FontSize = 100;
            this.IsSmallCaps = false;
            this.FontFamily = "Arial";
            this._FontColor = Colors.White;
            this.FontBrush = new SolidColorBrush(this._FontColor);
            this._BackgroundColor = Colors.Black;
            this.BackgroundBrush = new SolidColorBrush(this._BackgroundColor);
            this.ParseCaptionsSettings();
        }

        /// <summary>
        /// Initializes a new instance of the CaptionOptions class.
        /// </summary>
        /// <param name="other">the CaptionOptions from the Microsoft.CC608 DLL</param>
        internal CaptionOptions(Microsoft.CC608.CaptionOptions other)
        {
            this.BackgroundBrush = other.BackgroundBrush;
            this.CaptionWidth = other.CaptionWidth;
            this.FontBrush = other.FontBrush;
            this.FontFamily = other.FontFamily;
            this.FontSize = other.FontSize;
            this.IsSmallCaps = other.IsSmallCaps;
            this.VideoWidth = other.VideoWidth;
            this.WindowBrush = other.WindowBrush;
            this.ParseCaptionsSettings();
        }

        /// <summary>
        /// Gets the options for the Microsoft.CC608 Captions
        /// </summary>
        /// <returns>a new instance of the <see cref="Microsoft.CC608.CaptionOptions"/> class.</returns>
        internal Microsoft.CC608.CaptionOptions GetOptions()
        {
            return new Microsoft.CC608.CaptionOptions
            {
                BackgroundBrush = this.BackgroundBrush,
                CaptionWidth = this.CaptionWidth,
                FontBrush = this.FontBrush,
                FontFamily = this.FontFamily,
                FontSize = this.FontSize,
                IsSmallCaps = this.IsSmallCaps,
                VideoWidth = this.VideoWidth,
                WindowBrush = this.WindowBrush
            };
        }

        public void ParseCaptionsSettings()
        { 
            if (Windows.Media.ClosedCaptioning.ClosedCaptionProperties.FontColor != Windows.Media.ClosedCaptioning.ClosedCaptionColor.Default)
            {
                this._FontColor = Windows.Media.ClosedCaptioning.ClosedCaptionProperties.ComputedFontColor;
            }

            switch (Windows.Media.ClosedCaptioning.ClosedCaptionProperties.FontOpacity)
            {
                case Windows.Media.ClosedCaptioning.ClosedCaptionOpacity.OneHundredPercent:
                    this._FontColor.A = 255;
                    break;
                case Windows.Media.ClosedCaptioning.ClosedCaptionOpacity.SeventyFivePercent:
                    this._FontColor.A = 192;
                    break;
                case Windows.Media.ClosedCaptioning.ClosedCaptionOpacity.TwentyFivePercent:
                    this._FontColor.A = 64;
                    break;
                case Windows.Media.ClosedCaptioning.ClosedCaptionOpacity.ZeroPercent:
                    this._FontColor.A = 0;
                    break;
                case Windows.Media.ClosedCaptioning.ClosedCaptionOpacity.Default:
                    this._FontColor.A = 255;
                    break;
            }

            this.FontBrush = new SolidColorBrush(this._FontColor);

            if (Windows.Media.ClosedCaptioning.ClosedCaptionProperties.BackgroundColor != Windows.Media.ClosedCaptioning.ClosedCaptionColor.Default)
            {
                this._BackgroundColor = Windows.Media.ClosedCaptioning.ClosedCaptionProperties.ComputedBackgroundColor;
            }

            switch (Windows.Media.ClosedCaptioning.ClosedCaptionProperties.BackgroundOpacity)
            {
                case Windows.Media.ClosedCaptioning.ClosedCaptionOpacity.OneHundredPercent:
                    this._BackgroundColor.A = 255;
                    break;
                case Windows.Media.ClosedCaptioning.ClosedCaptionOpacity.SeventyFivePercent:
                    this._BackgroundColor.A = 192;
                    break;
                case Windows.Media.ClosedCaptioning.ClosedCaptionOpacity.TwentyFivePercent:
                    this._BackgroundColor.A = 64;
                    break;
                case Windows.Media.ClosedCaptioning.ClosedCaptionOpacity.ZeroPercent:
                    this._BackgroundColor.A = 0;
                    break;
                case Windows.Media.ClosedCaptioning.ClosedCaptionOpacity.Default:
                    this._BackgroundColor.A = 255;
                    break;
            }

            this.BackgroundBrush = new SolidColorBrush(this._BackgroundColor);

            if (Windows.Media.ClosedCaptioning.ClosedCaptionProperties.FontStyle != Windows.Media.ClosedCaptioning.ClosedCaptionStyle.Default)
            {
                switch (Windows.Media.ClosedCaptioning.ClosedCaptionProperties.FontStyle)
                {
                    case Windows.Media.ClosedCaptioning.ClosedCaptionStyle.Casual:
                        this.FontFamily = "Segoe Print";
                        break;
                    case Windows.Media.ClosedCaptioning.ClosedCaptionStyle.Cursive:
                        this.FontFamily = "Segoe Script";
                        break;
                    case Windows.Media.ClosedCaptioning.ClosedCaptionStyle.MonospacedWithoutSerifs:
                        this.FontFamily = "Lucida Sans Unicode";
                        break;
                    case Windows.Media.ClosedCaptioning.ClosedCaptionStyle.MonospacedWithSerifs:
                        this.FontFamily = "Courier New";
                        break;
                    case Windows.Media.ClosedCaptioning.ClosedCaptionStyle.ProportionalWithoutSerifs:
                        this.FontFamily = "Arial";
                        break;
                    case Windows.Media.ClosedCaptioning.ClosedCaptionStyle.ProportionalWithSerifs:
                        this.FontFamily = "Times New Roman";
                        break;
                    case Windows.Media.ClosedCaptioning.ClosedCaptionStyle.SmallCapitals:
                        this.FontFamily = "Arial";
                        this.IsSmallCaps = true;
                        break;
                }
            }

            if (Windows.Media.ClosedCaptioning.ClosedCaptionProperties.FontSize != Windows.Media.ClosedCaptioning.ClosedCaptionSize.Default)
            {
                switch (Windows.Media.ClosedCaptioning.ClosedCaptionProperties.FontSize)
                {
                    case Windows.Media.ClosedCaptioning.ClosedCaptionSize.FiftyPercent:
                        this.FontSize = 50;
                        break;
                    case Windows.Media.ClosedCaptioning.ClosedCaptionSize.OneHundredFiftyPercent:
                        this.FontSize = 150;
                        break;
                    case Windows.Media.ClosedCaptioning.ClosedCaptionSize.TwoHundredPercent:
                        this.FontSize = 200;
                        break;
                }
            }
        }

        /// <summary>
        /// Gets or sets the font family
        /// </summary>
	    public string FontFamily {get;set; }

        /// <summary>
        /// Gets or sets the font brush
        /// </summary>
        private Color _FontColor;
        public Brush FontBrush {get;set;}

        /// <summary>
        /// Gets or sets the background brush
        /// </summary>
        private Color _BackgroundColor;
        public Brush BackgroundBrush {get;set;}
 
        /// <summary>
        /// Gets or sets the window brush
        /// </summary>
        public Brush WindowBrush {get;set;}

        /// <summary>
        /// Gets or sets the caption width in pixels
        /// </summary>
        public double CaptionWidth {get;set;}

        /// <summary>
        /// Gets or sets the video width in pixels
        /// </summary>
        public double VideoWidth {get;set;}

        /// <summary>
        /// Gets the font size in % (50, 100, 150, 200)
        /// </summary>
        public int FontSize {get;set;}

        /// <summary>
        /// Gets or sets a value indicating whether the text is small caps
        /// </summary>
        public bool IsSmallCaps {get;set;}
    }
}
