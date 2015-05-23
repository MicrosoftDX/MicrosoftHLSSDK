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
  using System.Linq;
  using Microsoft.PlayerFramework.CaptionSettings;
  using Microsoft.PlayerFramework.CC608;
  using Windows.UI.Xaml.Media;

  /// <summary>
  /// FCC 608 Caption settings plug-in
  /// </summary>
  public class CC608CaptionSettingsPlugin : CaptionSettingsPluginBase
  {
    #region Fields
    /// <summary>
    /// the caption settings component
    /// </summary>
    private CC608CaptionSettings captionSettings = new CC608CaptionSettings();
    #endregion

    #region Constructors
    /// <summary>
    /// Initializes a new instance of the CC608CaptionSettingsPlugin class.
    /// </summary>
    public CC608CaptionSettingsPlugin()
    {
      this.DropShadowOffset = 2.0;
    }
    #endregion

    #region Properties
    /// <summary>
    /// Gets or sets the drop shadow offset
    /// </summary>
    public double DropShadowOffset
    {
      get
      {
        return this.captionSettings.DropShadowOffset;
      }

      set
      {
        this.captionSettings.DropShadowOffset = value;
      }
    }
    #endregion

    #region Methods
    /// <summary>
    /// Apply the caption settings
    /// </summary>
    /// <param name="settings">the new caption settings</param>
    public override void OnApplyCaptionSettings(PlayerFramework.CaptionSettings.Model.CustomCaptionSettings settings)
    {
      base.OnApplyCaptionSettings(settings);

      if (settings != null)
      {
        this.UpdateCaptionOptions(settings);
      }
    }

    /// <summary>
    /// Hook up the caption added even handler 
    /// </summary>
    /// <returns>true if the CC608Plugin is in the MediaPlayer</returns>
    protected override bool OnActivate()
    {
      CC608Plugin cc608plugIn = this.MediaPlayer.Plugins.OfType<CC608Plugin>().FirstOrDefault();

      if (cc608plugIn == null)
      {
        System.Diagnostics.Debug.WriteLine("You need to add the CC608PlugIn to enable 608 Caption Settings.");

        return false;
      }

      cc608plugIn.OnCaptionAdded += this.OnCaptionAdded;

      this.MediaPlayer.SizeChanged += this.MediaPlayer_SizeChanged;

      return base.OnActivate();
    }

    /// <summary>
    /// Detach the Caption Added event handler
    /// </summary>
    protected override void OnDeactivate()
    {
      if (this.MediaPlayer != null)
      {
        CC608Plugin cc608plugIn = this.MediaPlayer.Plugins.OfType<CC608Plugin>().FirstOrDefault();

        if (cc608plugIn != null)
        {
          cc608plugIn.OnCaptionAdded -= this.OnCaptionAdded;
        }

        this.MediaPlayer.SizeChanged -= this.MediaPlayer_SizeChanged;
      }

      base.OnDeactivate();
    }
    #endregion

    #region Implementation
    /// <summary>
    /// Update the Caption options
    /// </summary>
    /// <param name="settings">the custom caption settings</param>
    private void UpdateCaptionOptions(PlayerFramework.CaptionSettings.Model.CustomCaptionSettings settings)
    {
      CC608Plugin cc608plugIn = this.MediaPlayer.Plugins.OfType<CC608Plugin>().FirstOrDefault();

      if (cc608plugIn != null)
      {
        var fontFamily = GetFontFamilyName(settings.FontFamily);

        var captionOptions = new Microsoft.PlayerFramework.CC608.CaptionOptions
        {
          FontFamily = fontFamily == null ? string.Empty : fontFamily,
          FontSize = settings.FontSize.HasValue ? settings.FontSize.Value : 100,
          FontBrush = settings.FontColor == null ? null : new SolidColorBrush(settings.FontColor.ToColor()),
          BackgroundBrush = settings.BackgroundColor == null ? null : new SolidColorBrush(settings.BackgroundColor.ToColor()),
          VideoWidth = this.MediaPlayer.ActualWidth
        };

        if (settings.FontFamily == PlayerFramework.CaptionSettings.Model.FontFamily.Default ||
          settings.FontFamily == PlayerFramework.CaptionSettings.Model.FontFamily.MonospaceSansSerif ||
          settings.FontFamily == PlayerFramework.CaptionSettings.Model.FontFamily.MonospaceSerif)
        {
          captionOptions.CaptionWidth = 0;
        }
        else
        {
          double fontSize = 100;

          switch (captionOptions.FontSize)
          {
            case 50:
              fontSize = 75;
              break;
            case 100:
              break;
            case 150:
              fontSize = 120;
              break;
            case 200:
              fontSize = 130;
              break;
          }

          captionOptions.CaptionWidth = fontSize * this.MediaPlayer.ActualWidth * 0.5 / 100.0;
        }

        cc608plugIn.CaptionOptions = captionOptions;
      }
    }

    /// <summary>
    /// Update the caption settings the media player size changes
    /// </summary>
    /// <param name="sender">the Media Player</param>
    /// <param name="e">the size changed event arguments</param>
    private void MediaPlayer_SizeChanged(object sender, Windows.UI.Xaml.SizeChangedEventArgs e)
    {
      this.UpdateCaptionOptions(this.Settings);
    }

    /// <summary>
    /// Apply the settings when a caption element is added
    /// </summary>
    /// <param name="sender">the sender</param>
    /// <param name="e">the UI Element event arguments</param>
    private void OnCaptionAdded(object sender, UIElementEventArgs e)
    {
      this.captionSettings.ApplySettings(e.UIElement, this.Settings, null, true);
    }
    #endregion
  }
}
