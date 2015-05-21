using Microsoft.HLSClient;
using System;
using System.Collections.Generic;
using System.Net;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Input;
using Windows.UI.Xaml;

namespace Microsoft.PlayerFramework.Adaptive.HLS
{
  public class HLSPlugin : IPlugin
  {
    private List<IHLSAlternateRendition> _EmptyRenditionsList = new List<IHLSAlternateRendition>();

    /// <summary>
    /// Event handler fires when playback is about to begin.
    /// </summary>
    public event EventHandler<IHLSController> HLSControllerReady;

    /// <summary>
    /// Event handler called after the HLS client has suggested a bitrate switch.  
    /// The callee has an opportunity to cancel the switch by setting the 
    /// IHLSBitrateSwitchEventArgs.Cancel property to true.
    /// </summary>
    public event EventHandler<IHLSBitrateSwitchEventArgs> BitrateSwitchSuggested;

    /// <summary>
    /// Event handler called after the HLS client has completed bitrate switch.  
    /// </summary>
    public event EventHandler<IHLSBitrateSwitchEventArgs> BitrateSwitchCompleted;

    /// <summary>
    /// Event handler called after the HLS client has canceled the bitrate switch.  
    /// </summary>
    public event EventHandler<IHLSBitrateSwitchEventArgs> BitrateSwitchCancelled;

    /// <summary>
    /// Event raised when the HLS client switches between video, audio, or both.
    /// </summary>
    public event EventHandler<IHLSStreamSelectionChangedEventArgs> StreamSelectionChanged;

    /// <summary>
    /// Raised when the plugin has populated caption tracks. 
    /// This can be used to customize the display names, especially in the case of 608
    /// captions, where the names will be "CC1", "CC2", "CC3", and "CC4" In this event handler, 
    /// the app developer can remove 608
    /// caption tracks that are known to be empty, and rename caption tracks to more meaningful names.
    /// IMPORTANT: app developers should NOT change the Id value of the 608 caption objects,
    /// as these are used internally to identify the caption tracks.
    /// </summary>
    public event EventHandler AvailableCaptionsPopulated;

    /// <summary>
    /// Raised each time 608 data is found in the stream--used to signal the app that they should
    /// enable the captioning controls. (Only fires if the plugin is set for 608 captioning.)
    /// </summary>
    public event EventHandler Instream608CaptionsAvailable;

    /// <summary>
    /// Holds the desired startup bitrate.
    /// </summary>
    public uint? StartupBitrate { get; set; }

    /// <summary>
    /// Holds the desired minimum bitrate.
    /// </summary>
    private uint? _MinBitrate;
    public uint? MinBitrate
    {
      get { return _MinBitrate; }
      set
      {
        this._MinBitrate = value;
        try
        {
          if (value.HasValue && 
            this._Controller != null && 
            this._Controller.IsValid && 
            this._Controller.Playlist != null)
            this._Controller.Playlist.MinimumAllowedBitrate = value.Value;
        }
        catch  { }
      }
    }

    /// <summary>
    /// Holds the desired maximum bitrate.
    /// </summary>
    private uint? _MaxBitrate;
    public uint? MaxBitrate
    {
      get { return _MaxBitrate; }
      set
      {
        this._MaxBitrate = value;
        try
        {
          if (value.HasValue &&
            this._Controller != null &&
            this._Controller.IsValid &&
            this._Controller.Playlist != null)
            this._Controller.Playlist.MaximumAllowedBitrate = value.Value;
        }
        catch  { }
      }
    }

    private ClosedCaptionType _CaptionType;
    public ClosedCaptionType ClosedCaptionType
    {
      get { return _CaptionType; }
      set
      {
        if (value != this._CaptionType)
        {
          this._CaptionType = value;
          this.UpdateCaptionTracksAsync();
        }
      }
    }

    /// <summary>
    /// The HLSControllerFactory instance responsible for initializing the HLS Client.
    /// App developers would generally be interested in this only to wire up to the FIX ME event
    /// on the controller factory for the initial playlist request.
    /// </summary>
    private HLSControllerFactory _ControllerFactory;
    public IHLSControllerFactory ControllerFactory
    {
      get { return _ControllerFactory; }
    }

    /// <summary>
    /// Regsiter the byte stream handler.
    /// </summary>
    /// <param name="fileExtension">File extention to register.</param>
    /// <param name="mimeType">MIME type to register.</param>
    public void AddNonStandardRegistration(string fileExtension, string mimeType)
    {
      this.RegisterByteStreamHandler(fileExtension, mimeType);
    }

    private Windows.Foundation.Collections.IPropertySet _ControllerFactoryPropertySet;

    /// <summary>
    /// The HLSController instance that holds the keys to bitrates switching etc.
    /// Created and set by <see cref="_HLSControllerFactory"/> 
    /// </summary>
    private IHLSController _Controller;

    /// <summary>
    /// Downloads HLS WebVTT closed caption data.
    /// </summary>
    private HLSWebVTTCaptions _WebVTTCaptions;

    /// <summary>
    /// Constructor for the plugin
    /// </summary>
    public HLSPlugin()
    {
      // set this up in the constructor so that the property is available if the plugin is setup in code
      this._ControllerFactory = new HLSControllerFactory();

      this._ControllerFactoryPropertySet = 
        new Windows.Foundation.Collections.PropertySet() { {"ControllerFactory", this._ControllerFactory}};
    }

    /// <summary>
    /// Called when the HLS Client has initialized the <see cref="IHLSController"/>
    /// </summary>
    /// <param name="sender">The <see cref="IHLSControllerFactory"/> that was used to initialize the HLS Client.</param>
    /// <param name="controller">The <see cref="IHLSController"/> used to handle bitrate switching, etc.</param>
    private void HLSControllerFactory_HLSControllerReady(object sender, IHLSController controller)
    {
      try
      {
        this.UninitializeController();
        this._Controller = controller;
        this.InitializeController();

        if (null != this.HLSControllerReady)
          this.HLSControllerReady(sender, this._Controller);
      }
      catch
      {
        // Swallow these exceptions as 404s and other errors can cause the SDK code above to throw
        //throw;
      }
    }

    /// <summary>
    /// Uninitialize the controller.
    /// </summary>
    private void UninitializeController()
    {
      try
      {
        if (null != this._Controller && this._Controller.IsValid && null != this._Controller.Playlist)
        {
          this._Controller.Playlist.BitrateSwitchSuggested -= this.HLSPlaylist_BitrateSwitchSuggested;
          this._Controller.Playlist.BitrateSwitchCompleted -= this.HLSPlaylist_BitrateSwitchCompleted;
          this._Controller.Playlist.BitrateSwitchCancelled -= this.HLSPlaylist_BitrateSwitchCancelled;
          this._Controller.Playlist.StreamSelectionChanged -= this.HLSPlaylist_StreamSelectionChanged;
          this._Controller.Playlist.SegmentSwitched -= this.Playlist_SegmentSwitched;
        }
      }
      catch { }
    }

    /// <summary>
    /// Initialize the controller.
    /// </summary>
    private void InitializeController()
    {
      try
      {
        if (null != this._Controller && this._Controller.IsValid && null != this._Controller.Playlist)
        {
          this._Controller.Playlist.BitrateSwitchSuggested += this.HLSPlaylist_BitrateSwitchSuggested;
          this._Controller.Playlist.BitrateSwitchCompleted += this.HLSPlaylist_BitrateSwitchCompleted;
          this._Controller.Playlist.BitrateSwitchCancelled += this.HLSPlaylist_BitrateSwitchCancelled;
          this._Controller.Playlist.StreamSelectionChanged += this.HLSPlaylist_StreamSelectionChanged;
          this._Controller.Playlist.SegmentSwitched += this.Playlist_SegmentSwitched;

          if (this.StartupBitrate.HasValue)
            this._Controller.Playlist.StartBitrate = this.StartupBitrate.Value;

          if (this.MinBitrate.HasValue)
            this._Controller.Playlist.MinimumAllowedBitrate = this.MinBitrate.Value;

          if (this.MaxBitrate.HasValue)
            this._Controller.Playlist.MaximumAllowedBitrate = this.MaxBitrate.Value;
        }
      }
      catch { }
    }

    /// <summary>
    /// Called after the currently playing HLS playlist should switch bitrates.
    /// </summary>
    /// <param name="sender">The currently playling HLS playlist.</param>
    /// <param name="args">The <see cref="IHLSBitrateSwitchEventArgs"/> that contains the details for the 
    /// bitrate switch as well as the Cancel property which can be set to request that the switch be canceled.</param>
    private void HLSPlaylist_BitrateSwitchSuggested(object sender, IHLSBitrateSwitchEventArgs args)
    {
      if (null != this.BitrateSwitchSuggested)
        this.BitrateSwitchSuggested(sender, args);
    }

    /// <summary>
    /// Called after the currently playing HLS playlist has switched bitrates.
    /// </summary>
    /// <param name="sender">The currently playling HLS playlist.</param>
    /// <param name="args">The <see cref="IHLSBitrateSwitchEventArgs"/> that contains the details for the 
    /// bitrate switch.  Changes to the Cancel property will be ignored by the HLS Client.</param>
    private async void HLSPlaylist_BitrateSwitchCompleted(object sender, IHLSBitrateSwitchEventArgs args)
    {
      await this.RefreshStateAsync();

      if (null != this.BitrateSwitchCompleted)
        this.BitrateSwitchCompleted(sender, args);
    }

    /// <summary>
    /// Called when a bitrate switch has been canceled.
    /// </summary>
    /// <param name="sender">The currently playing HLS playlist.</param>
    /// <param name="args">The <see cref="IHLSBitrateSwitchEventArgs"/> that contains the details for the 
    /// canceled bitrate switch.</param>
    private void HLSPlaylist_BitrateSwitchCancelled(object sender, IHLSBitrateSwitchEventArgs args)
    {
      if (null != this.BitrateSwitchCancelled)
        this.BitrateSwitchCancelled(sender, args);
    }

    /// <summary>
    /// Called when the stream selection changes between audio, video or both.
    /// </summary>
    /// <param name="sender">The currently playing HLS playlist.</param>
    /// <param name="args">The <see cref="IHLSStreamSelectionChangedEventArgs"/> that describes the stream
    /// selection switch.</param>
    private void HLSPlaylist_StreamSelectionChanged(object sender, IHLSStreamSelectionChangedEventArgs args)
    {
      if (null != StreamSelectionChanged)
        this.StreamSelectionChanged(sender, args);
    }

    /// <summary>
    /// Called when playback moves from one segment to the next. We handle it here in order to 
    /// process the 608 data that may be available.
    /// </summary>
    /// <param name="sender">The currently playing HLS media playlist</param>
    /// <param name="args">Information about the completed segment (if not at the start) and the new segment</param>
    private async void Playlist_SegmentSwitched(object sender, IHLSSegmentSwitchEventArgs args)
    {
      await this.RefreshStateAsync();
      try
      {
        // only process if app selected 608 captions
        if (ClosedCaptionType.CC608Instream == this._CaptionType)
        {
          var toSegment = args.ToSegment;

          if (TrackType.AUDIO == toSegment.MediaType)
          {
            // no need to process audio track, as it will not contain 608 caption data
            return;
          }

          var payloads = toSegment.GetInbandCCUnits();

          // no data--nothing to process
          if (null == payloads)
            return;

          // raise the event to let listeners know that we have 608 data available
          if (null != Instream608CaptionsAvailable)
            this.Instream608CaptionsAvailable(this, EventArgs.Empty);

          // copy the data to a WinRT-compatible data structure
          var map = new Dictionary<ulong, IList<Byte>>();

          foreach (var payload in payloads)
          {
            //Debug.WriteLine("Found 608 payload data for timestamp: " + payload.Timestamp.ToString() + " ticks (" + (payload.Timestamp / 10000000.0).ToString() + " sec)" );
            var rawBytes = payload.GetPayload();
            if (null != rawBytes)
            {
              var bytes = new List<byte>(rawBytes);
              ulong key = (ulong)payload.Timestamp.Ticks;
              if (map.ContainsKey(key))
                map[key] = bytes;
              else
                map.Add(key, bytes);
            }
          }

          await this._MediaPlayer.Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
          {
            try
            {
              if (this._MediaPlayer.SelectedCaption != null)
              {
                // call this method on the selected caption object (if there is one)--this will in turn raise an 
                // event that the 608 caption plugin should be listening for...
                this._MediaPlayer.SelectedCaption.AugmentPayload(map, TimeSpan.Zero, TimeSpan.Zero);
              }

              // add the payload data to each caption object, in case it gets selected later
              foreach (var c in this._MediaPlayer.AvailableCaptions)
              {
                // make sure we have the queue ready--if not, create it
                var queue = c.Payload as Queue<Dictionary<ulong, IList<Byte>>>;
                if (null == queue)
                {
                  queue = new Queue<Dictionary<ulong, IList<Byte>>>();
                  c.Payload = queue;
                }

                // store the most recent data on the end of the queue
                queue.Enqueue(map);

                // but only keep the most recent three sets of data (as we could be as much as 30 seconds ahead)
                while (queue.Count > 3)
                  queue.Dequeue();
              }
            }
            catch { }
          });
        }
        else if (this._CaptionType == ClosedCaptionType.WebVTTSidecar && _WebVTTCaptions != null && _WebVTTCaptions.CurrentSubtitleId != null)
        {
          await this._MediaPlayer.Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, async () =>
          {
            try
            {
              await _WebVTTCaptions.DownloadAllSegmentsAsync(_WebVTTCaptions.CurrentSubtitleId);
            }
            catch { }
          });
        }
      }

      catch  { }
    }

    /// <summary>
    /// Occurs just before the MMPPF MediaPlayer source is set, cancel any closed caption downloads for the current video.
    /// </summary>
    /// <param name="sender">MMPPF media player.</param>
    /// <param name="e">Loading data.</param>
    private void MediaPlayer_MediaLoading(object sender, MediaPlayerDeferrableEventArgs e)
    {
      if (null != this._WebVTTCaptions)
      {
        this._WebVTTCaptions.Cancel();
        this._WebVTTCaptions = null;
      }
    }

    /// <summary>
    /// Occurs when the MediaElement has opened the media source audio or video.
    /// </summary>
    /// <param name="sender">MMPPF media player.</param>
    /// <param name="e">Not used.</param>
    private async void MediaPlayer_MediaOpened(object sender, RoutedEventArgs e)
    {
      try
      {
        await this.UpdateCaptionTracksAsync();
        await this.RefreshStateAsync();
      }
      catch { }
    }

    /// <summary>
    /// The selected closed caption has changed.
    /// </summary>
    /// <param name="sender">MMPPF media player.</param>
    /// <param name="e">Selected closed caption data.</param>
    private async void MediaPlayer_SelectedCaptionChanged(object sender, RoutedPropertyChangedEventArgs<Caption> e)
    {
      try
      {
        // only process for WebVTT captions
        if (ClosedCaptionType.WebVTTSidecar != this._CaptionType)
          return;

        if (null != this._WebVTTCaptions)
        {
          this._WebVTTCaptions.Cancel();
          this._WebVTTCaptions = null;
        }

        if (null != e.NewValue && this._Controller.IsValid)
        {
          this._WebVTTCaptions = new HLSWebVTTCaptions(this._MediaPlayer, _Controller);
          await this._WebVTTCaptions.DownloadAllSegmentsAsync(e.NewValue.Id);
        }
      }
      catch  { }
    }

    private void MediaPlayer_SelectedAudioStreamChanged(object sender, SelectedAudioStreamChangedEventArgs e)
    {
      var audioStreamWrapper = e.NewValue as AudioStreamWrapper;
      try
      {
        // either we got bad data, or the app developer has replaced the AudioStreamWrapper objects
        // either way, don't do anything
        if (null == audioStreamWrapper)
          return;

        foreach (var ar in GetAudioRenditions())
        {
          if (null == ar || ar.IsActive)
          {
            continue;
          }

          var id = AudioStreamWrapper.GetNamePlusLanguageForId(ar.Name, ar.Language);
          if (id == audioStreamWrapper.NamePlusLanguageId)
          {
            ar.IsActive = true;
            break;
          }
        }
      }
      catch  { }
    }


    /// <summary>
    /// Initializes the HLS Client by registering <see cref="Microsoft.HLSPlaylistHandler"/> as a byte
    /// stream handler and initializing an <see cref="HLSControllerFactory"/>
    /// </summary>
    private void InitalizePlugin()
    {
      this._ControllerFactory.HLSControllerReady += HLSControllerFactory_HLSControllerReady;

      // register the scheme handler for HTTP so that we can get the initial playlist
      this._MediaPlayer.MediaExtensionManager.RegisterSchemeHandler("Microsoft.HLSClient.HLSPlaylistHandler", "ms-hls:", _ControllerFactoryPropertySet);
      this._MediaPlayer.MediaExtensionManager.RegisterSchemeHandler("Microsoft.HLSClient.HLSPlaylistHandler", "ms-hls-s:", _ControllerFactoryPropertySet);

      // register both common MIME types
      this.RegisterByteStreamHandler(".m3u8", "application/vnd.apple.mpegurl");
      this.RegisterByteStreamHandler(".m3u8", "application/x-mpegurl");

      this._MediaPlayer.SelectedAudioStreamChanged += MediaPlayer_SelectedAudioStreamChanged;
      this._MediaPlayer.MediaLoading += MediaPlayer_MediaLoading;
      this._MediaPlayer.MediaOpened += MediaPlayer_MediaOpened;
      this._MediaPlayer.SelectedCaptionChanged += MediaPlayer_SelectedCaptionChanged;

      // Currently, the HLS SDK is not capable of handling the rapid-fire seek requests generated if 
      // seekWhileScrubbing is true, so turning it off by default.  If the HLS SDK is updated to tolerate
      // rapid-fire seek requests, this should be removed.
      this._MediaPlayer.SeekWhileScrubbing = false;
    }

    /// <summary>
    /// Uninitialize the plugin.
    /// </summary>
    private void UninitalizePlugin()
    {
      _ControllerFactory.HLSControllerReady -= HLSControllerFactory_HLSControllerReady;

      this._MediaPlayer.SelectedAudioStreamChanged -= MediaPlayer_SelectedAudioStreamChanged;
      this._MediaPlayer.MediaLoading -= MediaPlayer_MediaLoading;
      this._MediaPlayer.MediaOpened -= MediaPlayer_MediaOpened;
      this._MediaPlayer.SelectedCaptionChanged -= MediaPlayer_SelectedCaptionChanged;
    }

    /// <summary>
    /// Register the byte stream handler.
    /// </summary>
    /// <param name="fileExtension">File name extention that is registered.</param>
    /// <param name="mimeType">MIME type that is registered.</param>
    private void RegisterByteStreamHandler(string fileExtension, string mimeType)
    {
      if (this._MediaPlayer != null)
      {
        this._MediaPlayer.MediaExtensionManager.RegisterByteStreamHandler(
            "Microsoft.HLSClient.HLSPlaylistHandler",
            fileExtension, mimeType,
            _ControllerFactoryPropertySet);
      }
    }

    private async Task UpdateCaptionTracksAsync()
    {
      // can be called before the this._MediaPlayer is assigned, this is also 
      // called during MediaOpened which inits the caption track list
      if (null == this._MediaPlayer)
        return;

      await this._MediaPlayer.Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, async () =>
      {
        if (ClosedCaptionType.CC608Instream == this._CaptionType)
        {
          this._MediaPlayer.AvailableCaptions.Clear();
          this._MediaPlayer.SelectedCaption = null;

          // MMPPF will add the "Off" option automatically when a caption track is selected

          this._MediaPlayer.AvailableCaptions.Add(new Caption() { Id = "1", Description = "CC1" });
          this._MediaPlayer.AvailableCaptions.Add(new Caption() { Id = "2", Description = "CC2" });
          this._MediaPlayer.AvailableCaptions.Add(new Caption() { Id = "3", Description = "CC3" });
          this._MediaPlayer.AvailableCaptions.Add(new Caption() { Id = "4", Description = "CC4" });

          // raise event
          if (null != this.AvailableCaptionsPopulated)
            this.AvailableCaptionsPopulated(this, EventArgs.Empty);
        } 
        else if(this._CaptionType == ClosedCaptionType.WebVTTSidecar)
        {
          try
          {
            if (this._Controller != null &&
              this._Controller.IsValid &&
              this._Controller.Playlist != null &&
              this._Controller.Playlist.IsMaster)
            {
              this._WebVTTCaptions = new HLSWebVTTCaptions(this._MediaPlayer, this._Controller);
              await this._WebVTTCaptions.UpdateCaptionOptionsAsync();

              // raise event
              if (null != this.AvailableCaptionsPopulated)
                this.AvailableCaptionsPopulated(this, EventArgs.Empty);
            }
          }
          catch { }
        }
      });
    }

    /// <summary>
    /// Update the signal strength and media quality display.
    /// </summary>
    private async Task RefreshStateAsync()
    {
      await this._MediaPlayer.Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
      {
        if (null == this._Controller || !this._Controller.IsValid)
          return;
          try
          {
            // get the active variant stream into a local variable to avoid null references as it's null briefly while switching bitrates
            var activeVariantStream = this._Controller.Playlist.ActiveVariantStream;
            if (null == activeVariantStream)
              return;

            if (null != this._Controller.Playlist && this._Controller.Playlist.IsMaster)
            {
              // value ranging from 0 to 1
              this._MediaPlayer.SignalStrength = (0 == this._Controller.Playlist.MaximumAllowedBitrate) ? 0 :
                  (double)activeVariantStream.Bitrate / this._Controller.Playlist.MaximumAllowedBitrate;

              // HLS has an optional resolution, only set media quality if the HLS stream specifies a resolution.
              if (activeVariantStream.HasResolution)
                this._MediaPlayer.MediaQuality = (720 <= activeVariantStream.VerticalResolution) ?
                    MediaQuality.HighDefinition : MediaQuality.StandardDefinition;
            }
          }
          catch { }
      });
    }

    private MediaPlayer _MediaPlayer;
    MediaPlayer IPlugin.MediaPlayer
    {
      get { return this._MediaPlayer; }
      set { this._MediaPlayer = value; }
    }

    void IPlugin.Load()
    {
      this.InitalizePlugin();
    }

    void IPlugin.Unload()
    {
      this.UninitalizePlugin();
    }

    void IPlugin.Update(IMediaSource mediaSource)
    {
    }

    // TODO: Make sure that if GetAudioRenditions() doesn't return valid data (because the ActiveVariantStream was null) that we have some way 
    // of correcting this and getting the correct data soon thereafter. For example, consider calling this on every segment switch. 
    private IList<IHLSAlternateRendition> GetAudioRenditions()
    {
      var audioRenditions = this._EmptyRenditionsList;

      if (null != this._Controller && this._Controller.IsValid)
      {
        try
        {
          // get this value into a local variable as it can change and briefly be null during bitrate switching
          var activeVariantStream = this._Controller.Playlist.ActiveVariantStream;
          if (null != activeVariantStream && null != this._Controller.Playlist && this._Controller.Playlist.IsMaster)
            audioRenditions.AddRange(activeVariantStream.GetAudioRenditions());
        }
        catch { }
      }

      return audioRenditions;
    }
  }
}
