using Microsoft.HLSClient;
using Microsoft.PlayerFramework;
using System;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Media;

// The User Control item template is documented at http://go.microsoft.com/fwlink/?LinkId=234236

namespace HLSStreamMonitorWP
{

  public class PlaybackInfoOverlayData : INotifyPropertyChanged
  {

    public event PropertyChangedEventHandler PropertyChanged;

    private void OnPropertyChanged([CallerMemberName] string PropName="")
    {
      if (PropertyChanged != null)
        PropertyChanged(this, new PropertyChangedEventArgs(PropName));
    }

    private uint _curBitrate = 0;

    public uint CurrentBitrate
    {
      get { return _curBitrate; }
      set { _curBitrate = value; OnPropertyChanged(); }
    }

    private uint _curBandwidth = 0;

    public uint CurrentBandwidth
    {
      get { return _curBandwidth; }
      set { _curBandwidth = value; OnPropertyChanged(); }
    }

    private bool _IsBitrateShiftingUp = false;

    public bool IsBitrateShiftingUp
    {
      get { return _IsBitrateShiftingUp; }
      set { _IsBitrateShiftingUp = value; OnPropertyChanged(); }
    }

    private bool _IsBitrateShiftingDown = false;

    public bool IsBitrateShiftingDown
    {
      get { return _IsBitrateShiftingDown; }
      set { _IsBitrateShiftingDown = value; OnPropertyChanged(); }
    }

    private uint _toBitrate = 0;

    public uint ToBitrate
    {
      get { return _toBitrate; }
      set { _toBitrate = value; OnPropertyChanged(); }
    }

    private uint _curSegment = 0;

    public uint CurrentSegment
    {
      get { return _curSegment; }
      set { _curSegment = value; OnPropertyChanged(); }
    }

    private bool _showTotalSegmentCount = false;

    public bool ShowTotalSegmentCount
    {
      get { return _showTotalSegmentCount; }
      set { _showTotalSegmentCount = value; OnPropertyChanged(); }
    }

    private uint _totalSegmentCount;

    public uint TotalSegmentCount
    {
      get { return _totalSegmentCount; }
      set { _totalSegmentCount = value; OnPropertyChanged(); }
    }


    private bool _IsBitrateShiftPending = false;

    public bool IsBitrateShiftPending
    {
      get { return _IsBitrateShiftPending; }
      set { _IsBitrateShiftPending = value; OnPropertyChanged(); }
    }
    
  }
  public sealed partial class PlaybackInfoOverlayUI : UserControl
  {
    MediaPlayer _player = null;
    IHLSController _controller = null;
    PlaybackInfoOverlayData _data = null;
    DispatcherTimer _displayTimer = null;  
    public PlaybackInfoOverlayUI()
    {
      this.InitializeComponent();
      _data = new PlaybackInfoOverlayData();
    }

    public void Initialize(MediaPlayer player, IHLSController controller)
    {
      _player = player;
      _controller = controller;
      this.DataContext = _data;
      try
      {
        if (_controller.IsValid && _controller.Playlist != null)
        {
          _controller.Playlist.BitrateSwitchSuggested += Playlist_BitrateSwitchSuggested;
          _controller.Playlist.BitrateSwitchCompleted += Playlist_BitrateSwitchCompleted;
          _controller.Playlist.BitrateSwitchCancelled += Playlist_BitrateSwitchCancelled;
          _controller.Playlist.SegmentSwitched += Playlist_SegmentSwitched;
        }
      }
      catch (Exception Ex) { }

      _player.MediaOpened += _player_MediaOpened;
      _player.MediaFailed += _player_MediaFailed;
      _player.MediaEnded += _player_MediaEnded;
      _displayTimer =  new DispatcherTimer() { Interval = TimeSpan.FromSeconds(2) };
      _displayTimer.Tick += _displayTimer_Tick;
      _displayTimer.Start();
    }

    void _player_MediaEnded(object sender, RoutedEventArgs e)
    {
      if(_displayTimer.IsEnabled)
      _displayTimer.Stop();
    }

    void _player_MediaFailed(object sender, ExceptionRoutedEventArgs e)
    {
      if (_displayTimer.IsEnabled)
        _displayTimer.Stop();
    }

    void _displayTimer_Tick(object sender, object e)
    {
       
      try
      {
        if (_controller.IsValid && _controller.Playlist != null && _player.CurrentState == MediaElementState.Playing)
        {
          _data.CurrentBandwidth = _controller.GetLastMeasuredBandwidth();
        }
      }
      catch (Exception Ex) { }

    }

    void _player_MediaOpened(object sender, Windows.UI.Xaml.RoutedEventArgs e)
    {
      try
      {
        if (_controller != null && _controller.IsValid && _controller.Playlist.IsMaster && _controller.Playlist != null && _controller.Playlist.ActiveVariantStream != null)
          _data.CurrentBitrate = _controller.Playlist.ActiveVariantStream.Bitrate;
        if (_controller != null && _controller.IsValid && _controller.Playlist != null && _controller.Playlist.IsLive == false && _controller.Playlist.ActiveVariantStream != null && _controller.Playlist.ActiveVariantStream.Playlist != null)
        {
          _data.ShowTotalSegmentCount = true;
          _data.TotalSegmentCount = _controller.Playlist.ActiveVariantStream.Playlist.SegmentCount;
        }
      }
      catch(Exception Ex)
      {

      }
    }

    void Playlist_SegmentSwitched(IHLSPlaylist sender, IHLSSegmentSwitchEventArgs args)
    {
      _player.Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
      {
        try
        {
          _data.CurrentSegment = args.ToSegment.SequenceNumber; 
        }
        catch(Exception Ex)
        { }
      });
    }

    void Playlist_BitrateSwitchCancelled(IHLSPlaylist sender, IHLSBitrateSwitchEventArgs args)
    {
      _player.Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
      {
        _data.IsBitrateShiftPending = false;
        _data.IsBitrateShiftingDown = false;
        _data.IsBitrateShiftingUp = false;
      });
    }

    void Playlist_BitrateSwitchCompleted(IHLSPlaylist sender, IHLSBitrateSwitchEventArgs args)
    {
      _player.Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
      {
        try
        {
          if (_controller != null && _controller.IsValid && _controller.Playlist.IsMaster && _controller.Playlist != null && _controller.Playlist.ActiveVariantStream != null)
            _data.CurrentBitrate = _controller.Playlist.ActiveVariantStream.Bitrate;
          _data.IsBitrateShiftPending = false;
          _data.IsBitrateShiftingDown = false;
          _data.IsBitrateShiftingUp = false;
        }
        catch (Exception Ex) { }
      });
    }

    void Playlist_BitrateSwitchSuggested(IHLSPlaylist sender, IHLSBitrateSwitchEventArgs args)
    {
      _player.Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
      {
        try
        {
          _data.ToBitrate = args.ToBitrate;
          _data.IsBitrateShiftingDown = args.FromBitrate > args.ToBitrate;
          _data.IsBitrateShiftingUp = args.ToBitrate > args.FromBitrate;
          _data.IsBitrateShiftPending = true;
        }
        catch(Exception Ex)
        { }
      });
    }
  }
}
