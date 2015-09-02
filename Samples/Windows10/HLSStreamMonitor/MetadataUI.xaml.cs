using Microsoft.HLSClient;
using Microsoft.PlayerFramework;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Text;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.UI.Core;
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

  public class HLSMetadataID3Frame
  {
    private string _identifier = null;

    public string Identifier
    {
      get { return _identifier; }
    }

    private byte[] _framedata = null;

    public byte[] FrameData
    {
      get { return _framedata; }
    }

    public string FrameDataString
    {
      get
      {
        if (_framedata != null)
        {
         
          var stringsrc = _framedata.Select(b =>
          {
            return b == '\0' ? (byte)' ' : b;
          }).ToArray();
          return Encoding.UTF8.GetString(stringsrc.ToArray(), 0, stringsrc.Count());
        }
        else
          return null;
       
      }
    }

    public HLSMetadataID3Frame(IHLSID3TagFrame frm)
    {
      _identifier = frm.Identifier;
      _framedata = frm.GetFrameData();
    }

  }
  public class HLSMetadataPayload
  {
    private TimeSpan _timestamp = default(TimeSpan);

    public TimeSpan TimeStamp
    {
      get { return _timestamp; }
    }

    private uint _bitrate;

    public uint Bitrate
    {
      get { return _bitrate; } 
    }

    private uint _segmentsequence;

    public uint SegmentSequence
    {
      get { return _segmentsequence; } 
    }
    
    
    private List<HLSMetadataID3Frame> _frames = null;

    public List<HLSMetadataID3Frame> Frames
    {
      get { return _frames; }
    }

    public HLSMetadataPayload(IHLSID3MetadataPayload pld,uint bitrate,uint segsequence)
    {
      _timestamp = pld.Timestamp;
      _bitrate = bitrate;
      _segmentsequence = segsequence;
      var frames = pld.ParseFrames();
      if (frames != null && frames.Count > 0)
      {
        _frames = frames.Select((f) =>
        {
          return new HLSMetadataID3Frame(f);
        }).ToList();
      }
    }

  }

  public class HLSMetadataStream
  {



    public HLSMetadataStream(uint stmid)
    {
      _streamID = stmid;

    }

    private uint _streamID;

    public uint StreamID
    {
      get { return _streamID; }
    }

    private List<HLSMetadataPayload> _payloads = new List<HLSMetadataPayload>();

    public List<HLSMetadataPayload> Payloads
    {
      get { return _payloads; }
    }

    private ObservableCollection<HLSMetadataPayload> _displaypayloads = new ObservableCollection<HLSMetadataPayload>();

    public ObservableCollection<HLSMetadataPayload> DisplayPayloads
    {
      get { return _displaypayloads; }
    }

    public void DisplayMatching(TimeSpan position)
    {
      var matching = Payloads.Where((pld) => { return pld.TimeStamp <= position; }).ToList();

      foreach (var pld in matching)
      {
        DisplayPayloads.Insert(0, pld);
        Payloads.Remove(pld);
      }
    }


  }
  public sealed partial class MetadataUI : UserControl
  {
    ObservableCollection<HLSMetadataStream> _segmentid3tags = new ObservableCollection<HLSMetadataStream>();
    DispatcherTimer _event_timer = new DispatcherTimer() { Interval = TimeSpan.FromMilliseconds(1000 / 30) };
    public MetadataUI()
    {
      this.InitializeComponent();
      this.Loaded += MetadataUI_Loaded;
    }

    void MetadataUI_Loaded(object sender, RoutedEventArgs e)
    {
      _event_timer.Tick += _event_timer_Tick;
      lvStreams.ItemsSource = _segmentid3tags;
    }



    private IHLSController _controller;

    public IHLSController Controller
    {
      get { return _controller; }
      set
      {
        if (_controller != null && _controller.IsValid && _controller.Playlist != null)
        {
          _controller.Playlist.SegmentDataLoaded -= Playlist_SegmentDataLoaded;
          _controller.Playlist.SegmentSwitched -= Playlist_SegmentSwitched;
        }
        _controller = value;
        if (_controller.IsValid && _controller.Playlist != null)
        {
          _controller.Playlist.SegmentDataLoaded += Playlist_SegmentDataLoaded;
          _controller.Playlist.SegmentSwitched += Playlist_SegmentSwitched;
        }
      }
    }

    private MediaPlayer _player;

    public MediaPlayer Player
    {
      get { return _player; }
      set
      {

        if (_player != null)
        {
          _player.MediaOpened -= _player_MediaOpened;
          _player.MediaEnded -= _player_MediaEnded;
        }
        _player = value;
        if (_player != null)
        {
          _player.MediaOpened += _player_MediaOpened;
          _player.MediaEnded += _player_MediaEnded;
        }

      }
    }

    void _player_MediaEnded(object sender, MediaPlayerActionEventArgs e)
    {
      if(lvStreams.Items.Count == 0)
      {
        nometadataStatus.Visibility = Visibility.Collapsed;
        noplaybackStatus.Visibility = Visibility.Visible;
      }
      if (_event_timer.IsEnabled)
        _event_timer.Stop();
    }

    void _player_MediaOpened(object sender, RoutedEventArgs e)
    {
      noplaybackStatus.Visibility = Visibility.Collapsed;
      nometadataStatus.Visibility = Visibility.Visible;
      if (_event_timer.IsEnabled)
        _event_timer.Stop();
      _event_timer.Start();
    }


    void _event_timer_Tick(object sender, object e)
    {
      if (_player.CurrentState == MediaElementState.Playing)
      {
        foreach (var stm in _segmentid3tags)
          stm.DisplayMatching(_player.Position);
      }
    }

    void Playlist_SegmentSwitched(IHLSPlaylist sender, IHLSSegmentSwitchEventArgs args)
    {
      Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
      {
        try
        {
          var toSegment = args.ToSegment;
          if (toSegment.LoadState == SegmentState.LOADED)
          {
            var metadatastreams = toSegment.GetMetadataStreams();
            if (metadatastreams != null)
            {
              if (nometadataStatus.Visibility == Visibility.Visible)
                nometadataStatus.Visibility = Visibility.Collapsed;
              foreach (var stm in metadatastreams)
              {
                var payloads = stm.GetPayloads();
                if (payloads != null)
                {

                  var entry = _segmentid3tags.Where((hms) => { return hms.StreamID == stm.StreamID; }).FirstOrDefault();
                  if (entry == null)
                  {
                    entry = new HLSMetadataStream(stm.StreamID);
                    _segmentid3tags.Add(entry);
                  }
                  entry.Payloads.AddRange(payloads.Select((pld) => { return new HLSMetadataPayload(pld, args.ToBitrate, toSegment.SequenceNumber); }));
                }
              }
            }
          }
        }
        catch(Exception ex)
        {

        }
         
      });
    }

    void Playlist_SegmentDataLoaded(IHLSPlaylist sender, IHLSSegment args)
    {
      
    }

    private void lvStreams_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
      //if (lvStreams.SelectedItem != null)
      //  ccStreamData.Content = lvStreams.SelectedItem as HLSMetadataStream;
    }

  }
}
