
// The Blank Page item template is documented at http://go.microsoft.com/fwlink/?LinkId=391641
using HLSWP_BackgroundAudioSupport;
using Microsoft.HLSClient;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.Media;
using Windows.Media.Playback;
using Windows.UI.Core;
using Windows.UI.ViewManagement;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Media.Animation;
using Windows.UI.Xaml.Navigation;
using Windows.Graphics.Display;
using Microsoft.PlayerFramework.Adaptive.HLS;
using Windows.UI.Xaml.Media.Imaging;
namespace HLSStreamMonitorWP
{
  /// <summary>
  /// An empty page that can be used on its own or navigated to within a Frame.
  /// </summary>
  public sealed partial class MainPage : Page
  {

    IHLSControllerFactory _controllerFactory = null;
    IHLSController _controller = null;
    MediaExtensionManager _extmgr = null;
    DispatcherTimer _timerPlaybackControlDisplay = new DispatcherTimer() { Interval = TimeSpan.FromMilliseconds(250) };
    HLSSettings _settingsData = new HLSSettings();
    Dictionary<TrackType, ushort> _pidfilter = new Dictionary<TrackType, ushort>();
    TaskCompletionSource<bool> _tcsBackgroundInited = null;
    ulong _lastPos = 0;
    bool _isbackgroundtaskrunning = false;
    StreamInfo _si = null;
    HLSPlugin _hlsplugin = null;
    Uri _lastsrc = null;
    Rect _windowRect = default(Rect);
    Dictionary<Tuple<double, double>, uint> _restoBitRate = new Dictionary<Tuple<double, double>, uint>() { 
    { new Tuple<double,double>(double.MaxValue, 1081), 2500 * 1024 },  
    { new Tuple<double,double>(1080, 721), 2000 * 1024 },  
    { new Tuple<double,double>(720, 481), 1500 * 1024 },
    { new Tuple<double,double>(480, 0), 1300 * 1024 }};
    public MainPage()
    {
      this.InitializeComponent();
      this.NavigationCacheMode = NavigationCacheMode.Required;

      mePlayer.MediaOpened += mePlayer_MediaOpened;
      mePlayer.MediaFailed += mePlayer_MediaFailed;
      mePlayer.MediaEnded += mePlayer_MediaEnded;

      DisplayInformation.AutoRotationPreferences = DisplayOrientations.Landscape;
    }



    void mePlayer_CurrentStateChanged(object sender, RoutedEventArgs e)
    {

    }

    protected async override void OnNavigatedTo(NavigationEventArgs e)
    {

      App.Current.Suspending += Current_Suspending;
      App.Current.Resuming += Current_Resuming;
      Window.Current.Activated += Current_Activated;

      //get the HLS plugin from the PF player
      _hlsplugin = mePlayer.Plugins.FirstOrDefault(p => p is HLSPlugin) as HLSPlugin;
      _hlsplugin.ClosedCaptionType = ClosedCaptionType.CC608Instream;
      //get the controller factory
      _controllerFactory = _hlsplugin.ControllerFactory;
      //subscribe to HLSControllerReady
      _controllerFactory.HLSControllerReady += _controllerFactory_HLSControllerReady;

      _timerPlaybackControlDisplay.Tick += _timerPlaybackControlDisplay_Tick;

      _settingsData.PropertyChanged += _settingsData_PropertyChanged;


      ucHLSSettings.DataContext = _settingsData;

      btnFavorites_Click(null, null);

      _windowRect = ApplicationView.GetForCurrentView().VisibleBounds;
      _settingsData.MaximumBitrate = CalcMaxBitrate();
      ////await the background task
      await StartBackgroundTask();

     
    }

    void mediaElement_CurrentStateChanged(object sender, RoutedEventArgs e)
    {
      return;
    }


    void _timerPlaybackControlDisplay_Tick(object sender, object e)
    {
      try
      {
        if (mePlayer.CurrentState == MediaElementState.Playing && _controller != null && _controller.IsValid && _controller.Playlist != null && _controller.Playlist.IsLive)
        {
          var sw = _controller.Playlist.SlidingWindow;
          if (sw != null)
          {
            mePlayer.StartTime = sw.StartTimestamp;
            mePlayer.EndTime = sw.EndTimestamp;
            mePlayer.LivePosition = sw.LivePosition;
          }
        }
      }
      catch
      {

      }
    }

    uint CalcMaxBitrate()
    {
      double vh = Math.Min(_windowRect.Width, _windowRect.Height);
      if (_restoBitRate.Keys.Count(k => vh <= k.Item1 && vh > k.Item2) > 0)
      {
        return _restoBitRate[_restoBitRate.Keys.Where(k => vh <= k.Item1 && vh > k.Item2).First()];
      }
      else
        return _restoBitRate.Values.OrderBy(k => { return k; }).Last();
    }
    void _controllerFactory_HLSControllerReady(IHLSControllerFactory sender, IHLSController args)
    {

      _controller = args;

      ApplySettings();

      if (_controller.Playlist != null)
      {
        _controller.Playlist.StreamSelectionChanged += Playlist_StreamSelectionChanged;
      }

      Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
      {
        try
        {
          if (_controller != null && _controller.IsValid)
            ucPlaybackInfoOverlay.Initialize(mePlayer, _controller);
        }
        catch (Exception Ex)
        {

        }
      });

    }

    void Playlist_StreamSelectionChanged(IHLSPlaylist sender, IHLSStreamSelectionChangedEventArgs args)
    {
      Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () =>
      {
        try
        {


          if (_timerPlaybackControlDisplay.IsEnabled)
            _timerPlaybackControlDisplay.Stop();

          //if we switched to an audio only stream - show poster
          if (args.To == TrackType.AUDIO)
          {
            var hasme = VisualTreeHelper.FindElementsInHostCoordinates(new Point(LayoutRoot.ActualWidth / 2, LayoutRoot.ActualHeight / 2), LayoutRoot).
                                                      Where(uie => uie is MediaElement).FirstOrDefault();
            if (hasme != null)
            {
              MediaElement me = hasme as MediaElement;
              if (me.Parent is Panel)
              {
                Grid gridAudioOnly = new Grid()
                {
                  Name = "appAudioOnly",
                  HorizontalAlignment = Windows.UI.Xaml.HorizontalAlignment.Stretch,
                  VerticalAlignment = Windows.UI.Xaml.VerticalAlignment.Stretch,
                  Background = new SolidColorBrush(Windows.UI.Colors.DarkGray)
                };
                gridAudioOnly.Children.Add(new Image()
                {
                  Width = 200,
                  Height = 200,
                  Stretch = Stretch.Fill,
                  HorizontalAlignment = Windows.UI.Xaml.HorizontalAlignment.Center,
                  VerticalAlignment = Windows.UI.Xaml.VerticalAlignment.Center,
                  Source = new BitmapImage(new Uri("ms-appx:///assets/audioonly.png"))
                });
                (me.Parent as Panel).Children.Insert((me.Parent as Panel).Children.IndexOf(me) + 1, gridAudioOnly);

              }
            }

          }
          else if (args.To == TrackType.BOTH) //else hide poster if poster was being displayed
          {

            var hasme = VisualTreeHelper.FindElementsInHostCoordinates(new Point(mePlayer.ActualWidth / 2, mePlayer.ActualHeight / 2), mePlayer).
                                                       Where(uie => uie is MediaElement).FirstOrDefault();
            if (hasme != null)
            {
              MediaElement me = hasme as MediaElement;
              if (me.Parent is Panel)
                (me.Parent as Panel).Children.RemoveAt((me.Parent as Panel).Children.IndexOf(me) + 1);
            }
          }
        }
        catch (Exception Ex)
        {

        }
      });
    }


    void mePlayer_MediaEnded(object sender, Microsoft.PlayerFramework.MediaPlayerActionEventArgs e)
    {
      if (_timerPlaybackControlDisplay.IsEnabled)
        _timerPlaybackControlDisplay.Stop();
    }

    void mePlayer_MediaFailed(object sender, ExceptionRoutedEventArgs e)
    {
      mePlayer.Source = null;
      if (_timerPlaybackControlDisplay.IsEnabled)
        _timerPlaybackControlDisplay.Stop();
    }


    void mePlayer_MediaOpened(object sender, RoutedEventArgs e)
    {
      //playbackcontrol.Visibility = Visibility.Visible;
      ucPlaybackInfoOverlay.Visibility = Visibility.Visible;
      if (_si != null)
      {
        mePlayer.Position = TimeSpan.FromTicks((long)_si.Position);
        _si = null;
      }

      _timerPlaybackControlDisplay.Start();

     

    
    }


    void _settingsData_PropertyChanged(object sender, System.ComponentModel.PropertyChangedEventArgs e)
    {
      try
      {
        if (_controller == null || !_controller.IsValid) return;

        if (e.PropertyName == "IsWebVTTCaption" && _hlsplugin != null)
        {
          _hlsplugin.ClosedCaptionType = _settingsData.IsWebVTTCaption ? ClosedCaptionType.WebVTTSidecar : ClosedCaptionType.CC608Instream;
        }
        else if (e.PropertyName == "ForceAudioOnly")
        {
          _controller.TrackTypeFilter = _settingsData.ForceAudioOnly ? TrackType.AUDIO : TrackType.BOTH;
        }
        else if (e.PropertyName == "EnableAdaptiveBitrateMonitor")
        {
          _controller.EnableAdaptiveBitrateMonitor = _settingsData.EnableAdaptiveBitrateMonitor;
        }
        else if (e.PropertyName == "BitrateChangeNotificationInterval")
        {
          _controller.BitrateChangeNotificationInterval = _settingsData.BitrateChangeNotificationInterval;
        }
        else if (e.PropertyName == "UseTimeAveragedNetworkMeasure")
        {
          _controller.UseTimeAveragedNetworkMeasure = _settingsData.UseTimeAveragedNetworkMeasure;
        }
        else if (e.PropertyName == "StartBitrate")
        {
          _controller.Playlist.StartBitrate = _settingsData.StartBitrate;
        }
        else if (e.PropertyName == "MaximumBitrate")
        {
          _controller.Playlist.MaximumAllowedBitrate = _settingsData.MaximumBitrate == 0 ? _controller.Playlist.GetVariantStreams().Last().Bitrate : _settingsData.MaximumBitrate;
        }
        else if (e.PropertyName == "MinimumBitrate")
        {
          _controller.Playlist.MinimumAllowedBitrate = _settingsData.MinimumBitrate == 0 ? _controller.Playlist.GetVariantStreams().First().Bitrate : _settingsData.MinimumBitrate;
        }
        else if (e.PropertyName == "MinimumBufferLength")
        {
          _controller.MinimumBufferLength = _settingsData.MinimumBufferLength;
        }
        else if (e.PropertyName == "SegmentTryLimitOnBitrateSwitch")
        {
          _controller.SegmentTryLimitOnBitrateSwitch = _settingsData.SegmentTryLimitOnBitrateSwitch;
        }

        else if (e.PropertyName == "AllowSegmentSkipOnSegmentFailure")
        {
          _controller.AllowSegmentSkipOnSegmentFailure = _settingsData.AllowSegmentSkipOnSegmentFailure;
        }
        else if (e.PropertyName == "AudioPID")
        {
          if (_settingsData.AudioPID == 0)
          {
            if (_pidfilter.ContainsKey(TrackType.AUDIO))
            {
              _pidfilter.Remove(TrackType.AUDIO);
              _controller.Playlist.SetPIDFilter(_pidfilter);
            }
          }
          else
          {
            if ((_pidfilter.ContainsKey(TrackType.AUDIO) && _pidfilter[TrackType.AUDIO] != _settingsData.AudioPID) || !_pidfilter.ContainsKey(TrackType.AUDIO))
            {
              _pidfilter[TrackType.AUDIO] = _settingsData.AudioPID;
              _controller.Playlist.SetPIDFilter(_pidfilter);
            }
          }
        }
        else if (e.PropertyName == "VideoPID")
        {
          if (_settingsData.VideoPID == 0)
          {
            if (_pidfilter.ContainsKey(TrackType.VIDEO))
            {
              _pidfilter.Remove(TrackType.VIDEO);
              _controller.Playlist.SetPIDFilter(_pidfilter);
            }
          }
          else
          {
            if ((_pidfilter.ContainsKey(TrackType.VIDEO) && _pidfilter[TrackType.VIDEO] != _settingsData.VideoPID) || !_pidfilter.ContainsKey(TrackType.VIDEO))
            {
              _pidfilter[TrackType.VIDEO] = _settingsData.VideoPID;
              _controller.Playlist.SetPIDFilter(_pidfilter);
            }
          }
        }
      }
      catch (Exception ex) { }
    }

    void ApplySettings()
    {

      try
      {
        if (_controller == null || !_controller.IsValid) return;

        if (_hlsplugin != null)
        {
          _hlsplugin.ClosedCaptionType = _settingsData.IsWebVTTCaption ? ClosedCaptionType.WebVTTSidecar : ClosedCaptionType.CC608Instream;
        }

        _controller.TrackTypeFilter = _settingsData.ForceAudioOnly ? TrackType.AUDIO : TrackType.BOTH;

        _controller.EnableAdaptiveBitrateMonitor = _settingsData.EnableAdaptiveBitrateMonitor;

        _controller.BitrateChangeNotificationInterval = _settingsData.BitrateChangeNotificationInterval;

        _controller.UseTimeAveragedNetworkMeasure = _settingsData.UseTimeAveragedNetworkMeasure;

        if (_controller.Playlist.IsMaster)
        {
          if (_settingsData.StartBitrate != 0)
            _controller.Playlist.StartBitrate = _settingsData.StartBitrate;
          if (_settingsData.MaximumBitrate != 0)
            _controller.Playlist.MaximumAllowedBitrate = _settingsData.MaximumBitrate;
          if (_settingsData.MinimumBitrate != 0)
            _controller.Playlist.MinimumAllowedBitrate = _settingsData.MinimumBitrate;
        }

        _controller.MinimumBufferLength = _settingsData.MinimumBufferLength;

        _controller.SegmentTryLimitOnBitrateSwitch = _settingsData.SegmentTryLimitOnBitrateSwitch;

        _controller.AllowSegmentSkipOnSegmentFailure = _settingsData.AllowSegmentSkipOnSegmentFailure;

        _controller.UpshiftBitrateInSteps = _settingsData.UpshiftBitrateInSteps;

        if (_settingsData.AudioPID != 0)
        {

          if ((_pidfilter.ContainsKey(TrackType.AUDIO) && _pidfilter[TrackType.AUDIO] != _settingsData.AudioPID) || !_pidfilter.ContainsKey(TrackType.AUDIO))
          {
            _pidfilter[TrackType.AUDIO] = _settingsData.AudioPID;

          }
        }

        if (_settingsData.VideoPID != 0)
        {

          if ((_pidfilter.ContainsKey(TrackType.VIDEO) && _pidfilter[TrackType.VIDEO] != _settingsData.VideoPID) || !_pidfilter.ContainsKey(TrackType.VIDEO))
          {
            _pidfilter[TrackType.VIDEO] = _settingsData.VideoPID;
          }
        }

        if (_pidfilter.Count > 0)
          _controller.Playlist.SetPIDFilter(_pidfilter);
      }
      catch (Exception ex) { }

    }

    private void tglbtnInfo_Checked(object sender, RoutedEventArgs e)
    {
      if (ucPlaybackInfoOverlay != null)
        ucPlaybackInfoOverlay.Visibility = Visibility.Visible;
    }

    private void tglbtnInfo_Unchecked(object sender, RoutedEventArgs e)
    {
      if (ucPlaybackInfoOverlay != null)
        ucPlaybackInfoOverlay.Visibility = Visibility.Collapsed;
    }

    private void btnFavorites_Click(object sender, RoutedEventArgs e)
    {

      ucStreamSelection.Visibility = Visibility.Visible;

      ucStreamSelection.StreamSelected += (uc, url) =>
      {
        ucStreamSelection.Visibility = Visibility.Collapsed;
        if (url != null)
        {
          mePlayer.Source = new Uri(url);
        }
      };

    }


    private void btnSettings_Checked(object sender, RoutedEventArgs e)
    {
      ucHLSSettings.Visibility = Visibility.Visible;
    }

    private void btnSettings_Unchecked(object sender, RoutedEventArgs e)
    {
      ucHLSSettings.Visibility = Visibility.Collapsed;
    }

    private void btnRetry_Click(object sender, RoutedEventArgs e)
    {
      if (ucStreamSelection.Url != null && Uri.IsWellFormedUriString(ucStreamSelection.Url, UriKind.Absolute))
        mePlayer.Source = new Uri(ucStreamSelection.Url, UriKind.Absolute);
    }

    private void btnCancel_Click(object sender, RoutedEventArgs e)
    {
      btnFavorites_Click(null, null);
    }


    #region Background Audio Support

    async void Current_Activated(object sender, WindowActivatedEventArgs e)
    {
      if (e.WindowActivationState == CoreWindowActivationState.Deactivated)
      {
        OnForegroundDeactivated();
      }
      else
      {
        OnForegroundActivated();
      }
    }

    void Current_Resuming(object sender, object e)
    {
      OnAppResuming();
    }

    void Current_Suspending(object sender, Windows.ApplicationModel.SuspendingEventArgs e)
    {
      OnAppSuspending();
    }
    private async void OnForegroundDeactivated()
    {
      System.Diagnostics.Debug.WriteLine("Foreground: Deactivating");
      try
      {

        if (mePlayer.Source != null)
          _lastsrc = mePlayer.Source;

        //we are deactivating - if we are playing
        if (mePlayer.CurrentState == MediaElementState.Playing &&
          _controller != null &&
          _controller.IsValid &&
          _controller.Playlist != null)
        {
          if (_isbackgroundtaskrunning)
          {
            //let the task know we are deactivating and pass the stream state on
            ValueSet vs = new ValueSet();
            vs.Add("FOREGROUND_DEACTIVATED", new StreamInfo(mePlayer.Source.ToString(), (ulong)mePlayer.Position.Ticks).ToString());
            BackgroundMediaPlayer.SendMessageToBackground(vs);
            System.Diagnostics.Debug.WriteLine("Foreground: Deactivation message sent to background");
            //release the foreground extension
            mePlayer.Source = null;
            _controller = null;
          }
          else
          { 
            mePlayer.Source = null;
            _controller = null;
          }
        }
      }
      catch (Exception Ex) { }
    }

    private void OnForegroundActivated()
    {
      System.Diagnostics.Debug.WriteLine("Foreground: Activating");
      if (_isbackgroundtaskrunning)
      {
        //let the task know we are deactivating and pass the stream state on
        ValueSet vs = new ValueSet();
        vs.Add("FOREGROUND_ACTIVATED", "");
        BackgroundMediaPlayer.SendMessageToBackground(vs);
        System.Diagnostics.Debug.WriteLine("Foreground: Activation message sent to background");
      }
      else if(_lastsrc != null)
      { 
        mePlayer.Source = _lastsrc; 
      }
    }

    private void OnAppResuming()
    {
      System.Diagnostics.Debug.WriteLine("Foreground: Resuming");
      //we are resuming
      if (_isbackgroundtaskrunning)
      {
        //let the task know we are resuming  
        ValueSet vs = new ValueSet();
        vs.Add("FOREGROUND_RESUMED", "");
        BackgroundMediaPlayer.SendMessageToBackground(vs);
        System.Diagnostics.Debug.WriteLine("Foreground: Resumption message sent to background");
      }
    }

    private void OnAppSuspending()
    {
      System.Diagnostics.Debug.WriteLine("Foreground: Suspending");

      try
      {
        if (mePlayer.CurrentState == MediaElementState.Playing &&
          _controller != null &&
          _controller.IsValid &&
          _controller.Playlist != null &&
          _isbackgroundtaskrunning)
        {
          //let the task know we are deactivating and pass the stream state on
          ValueSet vs = new ValueSet();
          vs.Add("FOREGROUND_SUSPENDED", new StreamInfo(mePlayer.Source.ToString(), (ulong)mePlayer.Position.Ticks).ToString());
          BackgroundMediaPlayer.SendMessageToBackground(vs);
          System.Diagnostics.Debug.WriteLine("Foreground: Suspension message sent to background");
          //release the foreground extension
          mePlayer.Source = null;
        }
      }
      catch (Exception Ex) { }
    }

    private void OnBackgroundTaskInitialized()
    {
      System.Diagnostics.Debug.WriteLine("Foreground: Background Task Initialized");
      //release lock
      if (_tcsBackgroundInited != null)
        _tcsBackgroundInited.SetResult(true);
      _isbackgroundtaskrunning = true;
    }

    private void OnStreamStateInfoReceivedFromBackground(ValueSet vs)
    {
      System.Diagnostics.Debug.WriteLine("Foreground: Received stream state from background");
      Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () =>
      {
        System.Diagnostics.Debug.WriteLine("Foreground: Applying background stream state");
        _si = StreamInfo.FromString(vs["BACKGROUND_STREAMSTATE"] as string);
        if (_si != null)
        {
          mePlayer.Source = new Uri(_si.StreamURL);

        }
      });

    }

    private async Task OnBackgroundTaskCompletedOrCancelled()
    {
      System.Diagnostics.Debug.WriteLine("Foreground: Background task completed");
      await StartBackgroundTask();
    }
    private async void OnMessageReceivedFromBackground(object sender, MediaPlayerDataReceivedEventArgs e)
    {
      //successfully started up the background task  
      if (e.Data.Keys.Contains("INIT_BACKGROUND_DONE"))
      {
        OnBackgroundTaskInitialized();
      }
      else if (e.Data.Keys.Contains("BACKGROUND_STREAMSTATE"))
      {
        OnStreamStateInfoReceivedFromBackground(e.Data);
      }
      else if (e.Data.Keys.Contains("BACKGROUND_COMPLETED") || e.Data.Keys.Contains("BACKGROUND_CANCELLED"))
      {
        if (_isbackgroundtaskrunning == true)
        {
          _isbackgroundtaskrunning = false;
          await OnBackgroundTaskCompletedOrCancelled();
        }
        else if (_tcsBackgroundInited != null)
          await _tcsBackgroundInited.Task;
      }

    }

    private Task StartBackgroundTask()
    {
      //handle messages from the background
      BackgroundMediaPlayer.MessageReceivedFromBackground += OnMessageReceivedFromBackground;

      System.Diagnostics.Debug.WriteLine("Foreground: Background Task Initialization Requested");
      //launch the task by sending it a message
      ValueSet vs = new ValueSet();
      vs.Add("INIT_BACKGROUND", "");
      BackgroundMediaPlayer.SendMessageToBackground(vs);

      _tcsBackgroundInited = new TaskCompletionSource<bool>();
      //wait for reply from the background
      return _tcsBackgroundInited.Task;
    }
    #endregion
  }
}
