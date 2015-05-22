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
using Microsoft.HLSClient;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.PlayerFramework.Adaptive.HLS
{
  class LocatorEqualityComparer : IEqualityComparer<Tuple<uint, string>>
  {

    public bool Equals(Tuple<uint, string> x, Tuple<uint, string> y)
    {
      if (null == x || null == y || null == x.Item2 || null == y.Item2) return false;
      return (x.Item1 == y.Item1 && x.Item2.ToLowerInvariant() == y.Item2.ToLowerInvariant());
    }

    public int GetHashCode(Tuple<uint, string> obj)
    {
      return (obj.Item1.ToString() + " " + (obj.Item2 == null ? "" : obj.Item2)).GetHashCode();
    }
  }
  /// <summary>
  /// HSL WebVTT closed caption helper, updates list of available captions, downloads all closed caption 
  /// segments. Downloads segments asynchronously and sequentially to reduce the load placed on the player.
  /// </summary>
  class HLSWebVTTCaptions
  {
    private List<IHLSAlternateRendition> _EmptyRenditionsList = new List<IHLSAlternateRendition>();

    /// <summary>
    /// MMPPF media player that is used when updating closed caption data.
    /// </summary>
    private MediaPlayer _MediaPlayer;

    /// <summary>
    /// HLS Client controller that contains the closed caption metadata necessary to download all segments.
    /// </summary>
    private IHLSController _Controller;

    /// <summary>
    /// Flag used to cancel downloads.
    /// </summary>
    private bool _Cancel;

    private List<Tuple<uint, string>> _WebVTTLocators;

    public string CurrentSubtitleId;

    /// <summary>
    /// Initialize object.
    /// </summary>
    /// <param name="mediaPlayer">MMPPF media player.</param>
    /// <param name="controller">HLS Client controller.</param>
    public HLSWebVTTCaptions(MediaPlayer mediaPlayer, IHLSController controller)
    {
      this._MediaPlayer = mediaPlayer;
      this._Controller = controller;
      this._Cancel = false;
      this.CurrentSubtitleId = null;
    }

    /// <summary>
    /// Cancel current caption downloads.
    /// </summary>
    public void Cancel()
    {
      // Simple cancel, set flag and async code looks for flag before 
      // proceeding, instead of using a sync object and wait handle.
      this.CurrentSubtitleId = null;
      this._Cancel = true;
    }

    /// <summary>
    /// Update the closed caption options for the MMPPF media player.
    /// </summary>
    public async Task UpdateCaptionOptionsAsync()
    {
      await this._MediaPlayer.Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
      {
        foreach (var sr in this.GetSubtitleRenditions())
        {
          if (null == sr)
          {
            continue;
          }

          this._MediaPlayer.AvailableCaptions.Add(new Caption()
          {
            Id = sr.Name,
            Description = sr.Name,
            Language = sr.Language
          });
        }
      });
    }

    /// <summary>
    /// Download all segments for the closed caption.
    /// </summary>
    /// <param name="id">ID of the closed caption to download.</param>
    public async Task DownloadAllSegmentsAsync(string id)
    {
      try
      {
        IHLSSubtitleRendition subtitle = this.FindRendition(id);
        if (null != subtitle)
        {
          this.CurrentSubtitleId = id;
          await this.DownloadAllSegmentsAsync(subtitle);
        }
      }
      catch (NullReferenceException)
      {
        // Swallow exceptions here as 404s and other errors can cause the SDK code being accessed above to throw
      }
    }

    /// <summary>
    /// Download all segments for the closed caption.
    /// </summary>
    /// <param name="subtitle">Subtitle rendition of the closed caption to download.</param>
    /// <returns>Async task.</returns>
    private async Task DownloadAllSegmentsAsync(IHLSSubtitleRendition subtitle)
    {
      if (null == this._Controller || !this._Controller.IsValid || null == this._Controller.Playlist)
        return;

      uint segmentCount = await subtitle.RefreshAsync();

      if (0 == segmentCount)
        return;

      var locators = subtitle.GetSubtitleLocators();
      List<Tuple<uint, string>> toFetch = null;
      if (this._Controller.Playlist.IsLive && !(null == this._WebVTTLocators || 0 == this._WebVTTLocators.Count))
      {
        var locdata = locators.Select(l => new Tuple<uint, string>(l.Index, l.Location)).ToList();
        toFetch = locdata.Except(_WebVTTLocators, new LocatorEqualityComparer()).ToList();
        this._WebVTTLocators = locdata;
      }
      else
      {
        this._WebVTTLocators = locators.Select(l => new Tuple<uint, string>(l.Index, l.Location)).ToList();
        toFetch = this._WebVTTLocators;
      }


      foreach (var loc in toFetch)
      {
        if (this._Cancel)
          break;

        Uri uri;
        if (Uri.TryCreate(loc.Item2, UriKind.RelativeOrAbsolute, out uri))
          await this.DownloadSegmentAsync(loc.Item1, uri);
      }
    }

    /// <summary>
    /// Download single closed caption segment.
    /// </summary>
    /// <param name="segmentIndex">Closeed caption segment index to download.</param>
    /// <param name="segmentUri">Closed caption segment url to download.</param>
    /// <returns>Async task.</returns>
    private async Task DownloadSegmentAsync(uint segmentIndex, Uri segmentUri)
    {
      try
      {
        using (System.Net.Http.HttpClient client = new System.Net.Http.HttpClient())
        {
          var result = await client.GetStringAsync(segmentUri);
          if (!string.IsNullOrEmpty(result) && !this._Cancel)
          {
            await this._MediaPlayer.Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
            {
              //if (0 == segmentIndex || null == this._MediaPlayer.SelectedCaption.Payload)
              //  this._MediaPlayer.SelectedCaption.Payload = result;
              //else
              //  this._MediaPlayer.SelectedCaption.AugmentPayload(result, TimeSpan.Zero, TimeSpan.Zero);

              //this._MediaPlayer.SelectedCaption.AugmentPayload(result, TimeSpan.Zero, TimeSpan.Zero);

              this._MediaPlayer.SelectedCaption.AugmentPayload(result, ParseHLSWebVTTTimeStampOffset(result), TimeSpan.Zero);
            });
          }
        }
      }
      catch (Exception ex)
      {
        // continue if can't download segment captions
        System.Diagnostics.Debug.WriteLine(ex);
      }
    }

    private TimeSpan ParseHLSWebVTTTimeStampOffset(string payload)
    {
      
      var payloadupper = payload.ToUpperInvariant();
      var hdridx = payloadupper.IndexOf("X-TIMESTAMP-MAP");
      if (hdridx < 0) return TimeSpan.Zero;
      var localcuetimeentryidx = payloadupper.IndexOf("LOCAL:", hdridx);
      if (localcuetimeentryidx < 0) return TimeSpan.Zero;
      
      var ptsentryidx = payloadupper.IndexOf("MPEGTS:", hdridx);
      if (ptsentryidx < 0) return TimeSpan.Zero;
      

      string szcuetime = null;
      string szptstime = null;
      if (ptsentryidx > localcuetimeentryidx)
      {
        
        var commasepidx = payloadupper.IndexOf(",", localcuetimeentryidx);
        if (commasepidx < 0) return TimeSpan.Zero;


        int cuetimestartat = localcuetimeentryidx + ("LOCAL:").Length;
        szcuetime = payloadupper.Substring(cuetimestartat, commasepidx - cuetimestartat);

       
        int ptstimestartat = ptsentryidx + ("MPEGTS:").Length;
        var ca = payloadupper.Skip(ptstimestartat).TakeWhile(c => Char.IsNumber(c));
        if (ca == null || ca.Count() == 0) return TimeSpan.Zero;
        szptstime = new string(ca.ToArray());
       
      }
      else
      {
       
        int cuetimestartat = localcuetimeentryidx + ("LOCAL:").Length;
        var ca = payloadupper.Skip(cuetimestartat).TakeWhile(c => c != '\n' && c != '\r');
        if (ca == null || ca.Count() == 0) return TimeSpan.Zero;
        szcuetime = new string(ca.ToArray());
      
        int ptstimestartat = ptsentryidx + ("MPEGTS:").Length;
        var ca2 = payloadupper.Skip(ptstimestartat).TakeWhile(c => Char.IsNumber(c));
        if (ca2 == null || ca2.Count() == 0) return TimeSpan.Zero;
        szptstime = new string(ca2.ToArray());
        
      }

      if (string.IsNullOrEmpty(szcuetime) || string.IsNullOrEmpty(szptstime)) return TimeSpan.Zero;
      TimeSpan cuetime = TimeSpan.Zero;
      if (!TimeSpan.TryParse(szcuetime.Trim(), out cuetime)) return TimeSpan.Zero;
     
      long ptstime = 0;
      if (!Int64.TryParse(szptstime.Trim(), out ptstime)) return TimeSpan.Zero;

      System.Diagnostics.Debug.WriteLine("WebVTT HLS Timestamp offset = " + (TimeSpan.FromTicks(ptstime * (10000000 / 90000)) - cuetime).ToString());
      return TimeSpan.FromTicks(ptstime * (10000000 / 90000)) - cuetime;
    }
    /// <summary>
    /// Find the closed caption subtitle rendition for the id.
    /// </summary>
    /// <param name="id">Closed caption ID to find.</param>
    /// <returns>Subtitle rendition.</returns>
    private IHLSSubtitleRendition FindRendition(string id)
    {
      foreach (var st in this.GetSubtitleRenditions())
      {
        var subtitle = st as IHLSAlternateRendition;

        if (null != subtitle && id == subtitle.Name)
        {
          return st as IHLSSubtitleRendition;
        }
      }

      return null;
    }

    private IList<IHLSAlternateRendition> GetSubtitleRenditions()
    {
      if (null != this._Controller)
      {
        try
        {
          this._Controller.Lock();

          if (!this._Cancel &&
             null != this._Controller.Playlist &&
             this._Controller.Playlist.IsMaster &&
             null != this._Controller.Playlist.ActiveVariantStream)
            return this._Controller.Playlist.ActiveVariantStream.GetSubtitleRenditions();
        }
        catch (NullReferenceException)
        {
          // Swallow exceptions here as 404s and other errors can cause the SDK code being accessed above to throw
        }
        finally
        {
          this._Controller.Unlock();
        }
      }

      return this._EmptyRenditionsList;
    }
  }
}
