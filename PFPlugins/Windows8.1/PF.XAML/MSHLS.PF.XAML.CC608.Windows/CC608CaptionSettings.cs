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
namespace Microsoft.PlayerFramework.CC608.CaptionSettings
{
    using System;
    using System.Collections.Generic;
    using Microsoft.PlayerFramework.CaptionSettings;
    using Microsoft.PlayerFramework.CaptionSettings.Model;
#if WINDOWS_PHONE
    using System.Windows;
    using System.Windows.Controls;
    using Media = System.Windows.Media;
    using System.Windows.Documents;
    using ColorsNS = System.Windows.Media;
#else
    using Windows.UI;
    using Windows.UI.Xaml;
    using Windows.UI.Xaml.Controls;
    using Windows.UI.Xaml.Documents;
    using ColorsNS = Windows.UI;
    using Media = Windows.UI.Xaml.Media;
#endif
    using Model = Microsoft.PlayerFramework.CaptionSettings.Model;

    /// <summary>
    /// CC608 Caption application for <c>Xaml</c> (Windows 8.1 and Windows Phone 8)
    /// </summary>
    public class CC608CaptionSettings
    {
        #region Fields
        /// <summary>
        /// the font map
        /// </summary>
        private Dictionary<Microsoft.PlayerFramework.CaptionSettings.Model.FontFamily, Media.FontFamily> fontMap;
        #endregion

        #region Properties
        /// <summary>
        /// Gets or sets the drop shadow offset
        /// </summary>
        public double DropShadowOffset { get; set; }
        #endregion

        #region Methods
        /// <summary>
        /// Recursively apply the settings to the UI elements
        /// </summary>
        /// <param name="element">a UI Element</param>
        /// <param name="settings">the custom caption settings</param>
        /// <param name="parent">the parent element</param>
        /// <param name="isRoot">true if this is the root node</param>
        public void ApplySettings(UIElement element, CustomCaptionSettings settings, UIElement parent, bool isRoot = false)
        {
            if (settings == null)
            {
                return;
            }

            var panel       = element as StackPanel;
            var border      = element as Border;
            var textBlock   = element as TextBlock;
            var grid        = element as Grid;

            if (panel != null)
            {
                this.ApplyPanel(settings, isRoot, panel);
            }
            else if (border != null)
            {
                this.ApplyBorder(settings, border);
            }
            else if (textBlock != null)
            {
                this.ApplyText(settings, textBlock, parent);
            }
            else if (grid != null)
            {
                foreach (var item in grid.Children)
                {
                    this.ApplySettings(item, settings, grid, true);
                }
            }
            else
            {
                System.Diagnostics.Debugger.Break();
            }
        }
        #endregion

        #region Implementation
        /// <summary>
        /// Copy the attributes from the textBlock to the new text blocks
        /// </summary>
        /// <param name="textBlock">the original text block</param>
        /// <param name="newTextBlocks">the new text blocks</param>
        /// <param name="parent">the parent to add the new items to</param>
        /// <param name="opacity">the opacity to use</param>
        /// <param name="zIndex">the z index to use for the new text blocks</param>
        private static void CopyAttributes(TextBlock textBlock, TextBlock[] newTextBlocks, UIElement parent, double opacity, int zIndex = 1)
        {
            var border = parent as Border;

            Panel panel;

            if (border != null)
            {
                panel = new Grid();

                border.Child = panel;

                panel.Children.Add(textBlock);
            }
            else
            {
                panel = parent as Panel;
            }

            foreach (var item in newTextBlocks)
            {
                item.Foreground = new Media.SolidColorBrush(ColorsNS.Colors.Black);
                item.Text = textBlock.Text;
                item.FontFamily = textBlock.FontFamily;
                item.FontSize = textBlock.FontSize;
                item.LineHeight = textBlock.LineHeight;
                item.LineStackingStrategy = textBlock.LineStackingStrategy;
                item.Opacity = opacity;
                item.FontStyle = textBlock.FontStyle;
                item.FontWeight = textBlock.FontWeight;
                item.Margin = textBlock.Margin;

                Canvas.SetZIndex(item, zIndex);

                Typography.SetCapitals(item, Typography.GetCapitals(textBlock));

                panel.Children.Add(item);
            }
        }

        /// <summary>
        /// Determines if a row is empty
        /// </summary>
        /// <param name="row">the row</param>
        /// <returns>true if the row is empty, false otherwise</returns>
        private static bool IsRowEmpty(UIElement row)
        {
            var textBlock = row as TextBlock;

            if (textBlock != null)
            {
                return string.IsNullOrWhiteSpace(textBlock.Text);
            }

            var border = row as Border;

            if (border != null)
            {
                return IsRowEmpty(border.Child);
            }

            var stackPanel = row as StackPanel;

            if (stackPanel != null)
            {
                foreach (var item in stackPanel.Children)
                {
                    if (!IsRowEmpty(item))
                    {
                        return false;
                    }
                }

                return true;
            }

            return true;
        }

        /// <summary>
        /// Convert the color type to an opacity
        /// </summary>
        /// <param name="colorType">the color type</param>
        /// <returns>the opacity from 0-100</returns>
        private static uint? GetOpacity(ColorType colorType)
        {
            switch (colorType)
            {
                case ColorType.Default:
                    return null;

                case ColorType.Semitransparent:
                    return 50;

                case ColorType.Solid:
                    return 100;

                case ColorType.Transparent:
                    return 0;
            }

            return null;
        }

        /// <summary>
        /// Apply a drop shadow
        /// </summary>
        /// <param name="textBlock">the text block</param>
        /// <param name="parent">the parent</param>
        /// <param name="settings">the settings</param>
        private void ApplyDropShadow(TextBlock textBlock, UIElement parent, CustomCaptionSettings settings)
        {
            var offset = this.DropShadowOffset;

            if (settings.FontSize.HasValue)
            {
                offset = offset * settings.FontSize.Value / 100;
            }

            var newTextBlocks = new TextBlock[]
            {
                new TextBlock
                {
                    RenderTransform = new Media.TranslateTransform
                    {
                        X = offset
                    }
                },
                new TextBlock
                {
                    RenderTransform = new Media.TranslateTransform
                    {
                        Y = offset
                    }
                },
                new TextBlock
                {
                    RenderTransform = new Media.TranslateTransform
                    {
                        X = offset,
                        Y = offset
                    }
                },
            };

            CopyAttributes(textBlock, newTextBlocks, parent, 0.5, -1);
        }

        /// <summary>
        /// Apply settings to a border
        /// </summary>
        /// <param name="settings">the custom caption settings</param>
        /// <param name="border">a border</param>
        private void ApplyBorder(CustomCaptionSettings settings, Border border)
        {
            this.ApplySettings(border.Child, settings, border);
        }

        /// <summary>
        /// Apply settings to a panel
        /// </summary>
        /// <param name="settings">the custom caption settings</param>
        /// <param name="isRoot">is this the root panel</param>
        /// <param name="panel">the panel</param>
        private void ApplyPanel(CustomCaptionSettings settings, bool isRoot, StackPanel panel)
        {
            var children = System.Convert.ToDouble(panel.Children.Count);

            var startIndex = -1;

            var endIndex = -1;

            var index = 0;

            foreach (var item in panel.Children)
            {
                bool empty = IsRowEmpty(item);

                if (!empty && startIndex == -1)
                {
                    startIndex = index;
                }

                if (!empty)
                {
                    endIndex = index + 1;
                }

                this.ApplySettings(item, settings, panel);

                index++;
            }

            if (isRoot && settings.WindowColor != null)
            {
                if (startIndex >= 0 && endIndex >= 0)
                {
                    var startPercentage = Math.Max(0.0, (System.Convert.ToDouble(startIndex) / children) - 0.01);
                    var endPercentage = Math.Min(1.0, (System.Convert.ToDouble(endIndex) / children) + 0.01);

                    var gradientStops = new Media.GradientStopCollection();
                    var color = settings.WindowColor.ToColor();

                    gradientStops.Add(new Media.GradientStop
                        {
                            Color = Colors.Transparent,
                            Offset = startPercentage
                        });
                    gradientStops.Add(new Media.GradientStop
                    {
                        Color = color,
                        Offset = startPercentage
                    });
                    gradientStops.Add(new Media.GradientStop
                    {
                        Color = color,
                        Offset = endPercentage
                    });
                    gradientStops.Add(new Media.GradientStop
                    {
                        Color = Colors.Transparent,
                        Offset = endPercentage
                    });

                    panel.Background = new Media.LinearGradientBrush(gradientStops, 90);
                }
            }
        }

        /// <summary>
        /// Apply setting to a TextBlock
        /// </summary>
        /// <param name="settings">the custom caption settings</param>
        /// <param name="textBlock">the text block</param>
        /// <param name="parent">the parent</param>
        private void ApplyText(CustomCaptionSettings settings, TextBlock textBlock, UIElement parent)
        {
            if (string.IsNullOrWhiteSpace(textBlock.Text))
            {
                return;
            }

            this.ApplyFontStyle(settings, textBlock, parent);
        }

        /// <summary>
        /// Apply the font style
        /// </summary>
        /// <param name="settings">the caption settings</param>
        /// <param name="textBlock">the text block</param>
        /// <param name="parent">the text block's parent</param>
        private void ApplyFontStyle(CustomCaptionSettings settings, TextBlock textBlock, UIElement parent)
        {
            switch (settings.FontStyle)
            {
                case Model.FontStyle.Default:
                    // default style is Outline
                    this.ApplyOutline(textBlock, parent, settings);
                    break;

                case Model.FontStyle.DepressedEdge:
                    this.ApplyDepressedEdge(textBlock, parent, settings);
                    break;

                case Model.FontStyle.DropShadow:
                    this.ApplyDropShadow(textBlock, parent, settings);
                    break;

                case Model.FontStyle.Outline:
                    this.ApplyOutline(textBlock, parent, settings);
                    break;

                case Model.FontStyle.RaisedEdge:
                    this.ApplyRaisedEdge(textBlock, parent, settings);
                    break;

                case Model.FontStyle.None:
                    break;
            }
        }

        /// <summary>
        /// Apply a raised edge
        /// </summary>
        /// <param name="textBlock">the text block</param>
        /// <param name="parent">the parent</param>
        /// <param name="settings">the settings</param>
        private void ApplyRaisedEdge(TextBlock textBlock, UIElement parent, CustomCaptionSettings settings)
        {
            var offset = 1.5;

            if (settings.FontSize.HasValue)
            {
                offset = offset * settings.FontSize.Value / 100;
            }

            var newTextBlocks = new TextBlock[]
            {
                new TextBlock
                {
                    RenderTransform = new Media.TranslateTransform
                    {
                        X = offset,
                    },
                },
                new TextBlock
                {
                    RenderTransform = new Media.TranslateTransform
                    {
                        Y = offset
                    },
                },
                new TextBlock
                {
                    RenderTransform = new Media.TranslateTransform
                    {
                        X = offset,
                        Y = offset
                    },
                },
            };

            CopyAttributes(textBlock, newTextBlocks, parent, 0.25, 1);
        }

        /// <summary>
        /// Apply a depressed edge
        /// </summary>
        /// <param name="textBlock">the text block</param>
        /// <param name="parent">the parent</param>
        /// <param name="settings">the settings</param>
        private void ApplyDepressedEdge(TextBlock textBlock, UIElement parent, CustomCaptionSettings settings)
        {
            var offset = 1.5;

            if (settings.FontSize.HasValue)
            {
                offset = offset * settings.FontSize.Value / 100;
            }

            var newTextBlocks = new TextBlock[]
            {
                new TextBlock
                {
                    Name = "TopLeft",
                    RenderTransform = new Media.TranslateTransform
                    {
                        X = -offset,
                        Y = -offset
                    },
                },
                new TextBlock
                {
                    Name = "Top",
                    RenderTransform = new Media.TranslateTransform
                    {
                        Y = -offset
                    },
                },
                new TextBlock
                {
                    Name = "Left",
                    RenderTransform = new Media.TranslateTransform
                    {
                        X = -offset,
                    },
                },
            };

            CopyAttributes(textBlock, newTextBlocks, parent, 0.25, 1);
        }

        /// <summary>
        /// Apply an outline
        /// </summary>
        /// <param name="textBlock">the text block</param>
        /// <param name="parent">the parent</param>
        /// <param name="settings">the settings</param>
        private void ApplyOutline(TextBlock textBlock, UIElement parent, CustomCaptionSettings settings)
        {
            var offset = 1.0;

            if (settings.FontSize.HasValue)
            {
                offset = settings.FontSize.Value / 100;
            }
            
            var newTextBlocks = new TextBlock[]
            {
                new TextBlock
                {
                    RenderTransform = new Media.TranslateTransform
                    {
                        X = -offset,
                        Y = -offset
                    },
                },
                new TextBlock
                {
                    RenderTransform = new Media.TranslateTransform
                    {
                        Y = -offset
                    },
                },
                new TextBlock
                {
                    RenderTransform = new Media.TranslateTransform
                    {
                        X = offset,
                        Y = -offset
                    },
                },
                new TextBlock
                {
                    RenderTransform = new Media.TranslateTransform
                    {
                        X = -offset,
                    },
                },
                new TextBlock
                {
                    RenderTransform = new Media.TranslateTransform
                    {
                        X = offset,
                    },
                },
                new TextBlock
                {
                    RenderTransform = new Media.TranslateTransform
                    {
                        X = -offset,
                        Y = offset
                    },
                },
                new TextBlock
                {
                    RenderTransform = new Media.TranslateTransform
                    {
                        Y = offset
                    },
                },
                new TextBlock
                {
                    RenderTransform = new Media.TranslateTransform
                    {
                        X = offset,
                        Y = offset
                    }
                },
            };

            CopyAttributes(textBlock, newTextBlocks, parent, textBlock.Opacity, -1);
        }

        /// <summary>
        /// Gets the Windows or Windows Phone font family from the user 
        /// settings Font
        /// </summary>
        /// <param name="settings">the custom caption settings</param>
        /// <returns>the font family</returns>
        private Media.FontFamily GetFontFamily(CustomCaptionSettings settings)
        {
            if (this.fontMap == null)
            {
                this.fontMap = new Dictionary<Microsoft.PlayerFramework.CaptionSettings.Model.FontFamily, Media.FontFamily>();
            }

            Media.FontFamily fontFamily;

            if (this.fontMap.TryGetValue(settings.FontFamily, out fontFamily))
            {
                return fontFamily;
            }

            string fontName = CC608CaptionSettingsPlugin.GetFontFamilyName(settings.FontFamily);

            if (fontName == null)
            {
                return null;
            }

            fontFamily = new Media.FontFamily(fontName);

            this.fontMap[settings.FontFamily] = fontFamily;

            return fontFamily;
        }

        #endregion
    }
}
