// <copyright file="CaptionOption.cs" company="Microsoft Corporation">
// Copyright (c) 2014 Microsoft Corporation All Rights Reserved
// </copyright>
// <author>Michael S. Scherotter</author>
// <email>mischero@microsoft.com</email>
// <date>2014-01-27</date>
// <summary>Caption Options</summary>

namespace Microsoft.PlayerFramework.CC608
{
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
            this.IsSmallCaps = false;
            this.FontSize = 100;
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

        /// <summary>
        /// Gets or sets the font family
        /// </summary>
	    public string FontFamily {get;set;}

        /// <summary>
        /// Gets or sets the font brush
        /// </summary>
        public Brush FontBrush {get;set;}

        /// <summary>
        /// Gets or sets the background brush
        /// </summary>
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
