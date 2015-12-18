using Microsoft.HLSClient;
using Microsoft.PlayerFramework;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;

// The User Control item template is documented at http://go.microsoft.com/fwlink/?LinkId=234236

namespace HLSStreamMonitor
{
  public sealed partial class PlaybackStatusUI : UserControl
  {
    PlaybackStatusData _data = null;
    MediaPlayer _player = null;
    IHLSController _controller = null;
    bool _goingLive = false;
    public PlaybackStatusUI()
    {
      this.InitializeComponent();
    }

    public void Initialize(IHLSController controller, MediaPlayer player)
    {
      _data = new PlaybackStatusData(controller, player);
      this.DataContext = _data;
      _player = player;
      _controller = controller;

      _player.MediaOpened += _player_MediaOpened;
      _player.SeekCompleted += _player_SeekCompleted;
      _controller.PrepareResourceRequest += _controller_PrepareResourceRequest;


    }

    void _player_SeekCompleted(object sender, RoutedEventArgs e)
    {
      if(_goingLive)
      {
        _goingLive = false;
        btnGoLive.Visibility = Windows.UI.Xaml.Visibility.Collapsed;
        return;
      }
      if(_controller != null && _controller.IsValid 
        && _controller.Playlist != null 
        && _controller.Playlist.IsLive &&
        _controller.Playlist.SlidingWindow != null)
      {
        if (_player.Position < _controller.Playlist.SlidingWindow.LivePosition)
          btnGoLive.Visibility = Windows.UI.Xaml.Visibility.Visible;
      }
    }

    private void btnGoLive_Click(object sender, RoutedEventArgs e)
    {
      if (_controller != null && _controller.IsValid
       && _controller.Playlist != null 
       && _controller.Playlist.IsLive)
      {
        _goingLive = true;
        _player.Position = _controller.Playlist.SlidingWindow.LivePosition + TimeSpan.FromMilliseconds(500);
      }
    }
    void _controller_PrepareResourceRequest(IHLSController sender, IHLSResourceRequestEventArgs args)
    {
      try
      {
        //if there is a key request - display an encrypted flag
        if (args.Type == ResourceType.KEY)
        {
          Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () => { tblkEncryption.Text = "Encrypted"; });
        }
        //always submit to prevent locking up the runtime
        args.Submit();
      }
      catch(Exception Ex)
      {

      }
    }

    public void Reset()
    {
      if (_data != null) _data.Reset();
      if (_player != null) _player.MediaOpened -= _player_MediaOpened;
      if (_controller != null) _controller.PrepareResourceRequest -= _controller_PrepareResourceRequest;
      _data = null;
      this.DataContext = null;

      noplaybackStatus.Visibility = Visibility.Visible;
    }

    void _player_MediaOpened(object sender, RoutedEventArgs e)
    {
      noplaybackStatus.Visibility = Visibility.Collapsed;
      if (_controller.Playlist.IsLive) //display stream type
      {
        spSegmentTotalCount.Visibility = Visibility.Collapsed;
        tblkStreamType.Text = "Live";
        tblkLiveStreamType.Text = _controller.Playlist.PlaylistType == HLSPlaylistType.EVENT ? "(Event)" : "(Sliding Window)";
      }
      else
      {
        spSegmentTotalCount.Visibility = Visibility.Visible;
        tblkStreamType.Text = "VOD";
        tblkLiveStreamType.Text = "";
      }

    }

   
  }

  public class PlaybackStatusData : INotifyPropertyChanged
  {
    private DispatcherTimer _clocktimer;
    IHLSController _controller = null;
    MediaPlayer _player = null;
    TimeSpan _initialPlaybackPosition = default(TimeSpan);
    public PlaybackStatusData(IHLSController controller, MediaPlayer player)
    {
      _controller = controller;
      _player = player;
      _clocktimer = new DispatcherTimer() { Interval = TimeSpan.FromMilliseconds(250) };
      _clocktimer.Tick += _clocktimer_Tick;


      player.MediaOpened += player_MediaOpened;
      _player.MediaEnded += _player_MediaEnded;
      _player.MediaFailed += _player_MediaFailed;
      _controller.Playlist.BitrateSwitchSuggested += Playlist_BitrateSwitchSuggested;
      _controller.Playlist.BitrateSwitchCompleted += Playlist_BitrateSwitchCompleted;
      _controller.Playlist.BitrateSwitchCancelled += Playlist_BitrateSwitchCancelled;
      _controller.Playlist.SegmentSwitched += Playlist_SegmentSwitched;

      _controller.Playlist.SlidingWindowChanged += Playlist_SlidingWindowChanged;
    }
 

    void Playlist_SlidingWindowChanged(IHLSPlaylist sender, IHLSSlidingWindow args)
    {
      try
      {
        if (_initialPlaybackPosition == default(TimeSpan) && args.StartTimestamp != default(TimeSpan))
          _initialPlaybackPosition = args.StartTimestamp;
      }
      catch(Exception Ex)
      {

      }
    }

    void _player_MediaFailed(object sender, ExceptionRoutedEventArgs e)
    {
      _clocktimer.Stop();
    }

    void _player_MediaEnded(object sender, MediaPlayerActionEventArgs e)
    {
      _clocktimer.Stop();
    }

    public void Reset()
    {
      _clocktimer.Stop();
      _player.MediaOpened -= player_MediaOpened;
      _player.MediaEnded -= _player_MediaEnded;
      _player.MediaFailed -= _player_MediaFailed;

      if (_controller != null && _controller.IsValid)
      {
        _controller.Playlist.BitrateSwitchSuggested -= Playlist_BitrateSwitchSuggested;
        _controller.Playlist.BitrateSwitchCompleted -= Playlist_BitrateSwitchCompleted;
        _controller.Playlist.BitrateSwitchCancelled -= Playlist_BitrateSwitchCancelled;
        _controller.Playlist.SegmentSwitched -= Playlist_SegmentSwitched;

        _controller.Playlist.SlidingWindowChanged -= Playlist_SlidingWindowChanged;
      }
    }


    void player_MediaOpened(object sender, RoutedEventArgs e)
    {
      if (_initialPlaybackPosition == default(TimeSpan))
        _initialPlaybackPosition = _player.Position;

      try
      {
        if (_controller != null && _controller.IsValid && _controller.Playlist.ActiveVariantStream != null && _controller.Playlist.IsMaster)
        {
          _currentBitrate = _controller.Playlist.ActiveVariantStream.Bitrate;
        }
      }
      catch (Exception ex)
      {

      }


      _player.Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
      {

        if (PropertyChanged != null)
        {
          PropertyChanged(this, new PropertyChangedEventArgs("CurrentBitrate"));
        }
      });

      _clocktimer.Start();
    }



    void Playlist_SegmentSwitched(IHLSPlaylist sender, IHLSSegmentSwitchEventArgs args)
    {
      try
      {
        if (_controller.Playlist.ActiveVariantStream != null && _controller.Playlist.ActiveVariantStream.Playlist != null && !_controller.Playlist.IsLive && _totalSegments == 0)
        {
          _totalSegments = _controller.Playlist.ActiveVariantStream.Playlist.SegmentCount;
          _player.Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
          {

            if (PropertyChanged != null)
            {
              PropertyChanged(this, new PropertyChangedEventArgs("TotalSegments"));
            }
          });

        }
        else if (!_controller.Playlist.IsMaster)
        {
          _totalSegments = _controller.Playlist.SegmentCount;
          _player.Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
          {

            if (PropertyChanged != null)
            {
              PropertyChanged(this, new PropertyChangedEventArgs("TotalSegments"));
            }
          });
        }

        _currentSegment = args.ToSegment.SequenceNumber;
        _player.Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
        {

          if (PropertyChanged != null)
          {
            PropertyChanged(this, new PropertyChangedEventArgs("CurrentSegment"));
          }
        });
      }
      catch(Exception ex)
      {

      }
    }

    void Playlist_BitrateSwitchCancelled(IHLSPlaylist sender, IHLSBitrateSwitchEventArgs args)
    {
      _player.Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
      {

        _downshiftPending = false;
        _upshiftPending = false;
        if (PropertyChanged != null)
        {
          PropertyChanged(this, new PropertyChangedEventArgs("UpshiftPending"));
          PropertyChanged(this, new PropertyChangedEventArgs("DownshiftPending"));
        }
      });
    }

    void Playlist_BitrateSwitchCompleted(IHLSPlaylist sender, IHLSBitrateSwitchEventArgs args)
    {
      _player.Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
      {
        try
        {
          _downshiftPending = false;
          _upshiftPending = false;
          _currentBitrate = args.ToBitrate;
          if (PropertyChanged != null)
          {
            PropertyChanged(this, new PropertyChangedEventArgs("CurrentBitrate"));
            PropertyChanged(this, new PropertyChangedEventArgs("UpshiftPending"));
            PropertyChanged(this, new PropertyChangedEventArgs("DownshiftPending"));
          }
        }
        catch(Exception Ex)
        {

        }
      });
    }

    void Playlist_BitrateSwitchSuggested(IHLSPlaylist sender, IHLSBitrateSwitchEventArgs args)
    {
      _player.Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
        {
          try
          {
            if (args.FromBitrate > args.ToBitrate)
            {
              _downshiftPending = true;
              _upshiftPending = false;
            }
            else
            {
              _upshiftPending = true;
              _downshiftPending = false;
            }

            _targetBitrate = args.ToBitrate;
            if (PropertyChanged != null)
            {
              PropertyChanged(this, new PropertyChangedEventArgs("TargetBitrate"));
              PropertyChanged(this, new PropertyChangedEventArgs("UpshiftPending"));
              PropertyChanged(this, new PropertyChangedEventArgs("DownshiftPending"));
            }
          }
          catch(Exception Ex)
          {

          }
        });
    }



    void _clocktimer_Tick(object sender, object e)
    {
      _timeNow = DateTime.Now;

      try
      {
        if (_player.CurrentState == MediaElementState.Playing && _controller != null && _controller.IsValid)
        {
          if (_controller.Playlist.IsLive == false)
          {
            _timePlayed = _player.Position.Subtract(_initialPlaybackPosition);
          }

          else
          {
            _timeLive = _player.Position;
            if (_initialPlaybackPosition != default(TimeSpan))
              _timePlayed = _player.Position.Subtract(_initialPlaybackPosition);

            var sw = _controller.Playlist.SlidingWindow;
            if (sw != null)
            {
              _player.StartTime = sw.StartTimestamp;
              _player.EndTime = sw.EndTimestamp;
              _player.LivePosition = sw.LivePosition;
            }

            //System.Diagnostics.Debug.WriteLine(string.Format("Player Position : {0}, Sliding Window Start : {1}, Sliding Window End : {2}, Live Position : {3}", _player.Position, sw.StartTimestamp, sw.EndTimestamp, sw.LivePosition));
          }
          if (_controller.IsValid)
            _currentBandwidth = _controller.GetLastMeasuredBandwidth();
        }
      }
      catch(Exception Ex)
      {

      }
      

      if (PropertyChanged != null)
      {
        PropertyChanged(this, new PropertyChangedEventArgs("TimeNow"));
        PropertyChanged(this, new PropertyChangedEventArgs("TimePlayed"));
        PropertyChanged(this, new PropertyChangedEventArgs("TimeLive"));
        PropertyChanged(this, new PropertyChangedEventArgs("CurrentBandwidth"));
      }
    }

    private DateTime _timeNow;

    public DateTime TimeNow
    {
      get { return _timeNow; }
    }


    private TimeSpan _timePlayed;

    public TimeSpan TimePlayed
    {
      get { return _timePlayed; }
    }

    private TimeSpan _timeLive;

    public TimeSpan TimeLive
    {
      get { return _timeLive; }
    }


    private uint _currentBandwidth;

    public uint CurrentBandwidth
    {
      get { return _currentBandwidth; }
    }

    private ulong _currentBitrate;

    public ulong CurrentBitrate
    {
      get { return _currentBitrate; }
    }

    private ulong _targetBitrate;

    public ulong TargetBitrate
    {
      get { return _targetBitrate; }
    }

    private bool _upshiftPending;

    public bool UpshiftPending
    {
      get { return _upshiftPending; }
    }

    private bool _downshiftPending;

    public bool DownshiftPending
    {
      get { return _downshiftPending; }
    }

    private uint _currentSegment;

    public uint CurrentSegment
    {
      get { return _currentSegment; }
    }

    private uint _totalSegments;

    public uint TotalSegments
    {
      get { return _totalSegments; }
    }


    public event PropertyChangedEventHandler PropertyChanged;
  }


}
