using Microsoft.HLSClient;
using Microsoft.PlayerFramework;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
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

  public class UnprocessedTagForSegment
  {
    private uint _forSegmentSequence;

    public uint ForSegmentSequence
    {
      get { return _forSegmentSequence; }
      set { _forSegmentSequence = value; }
    }

    private List<string> _tags = new List<string>();

    public List<string> Tags
    {
      get { return _tags; }
      set { _tags = value; }
    } 
  }
  public sealed partial class UnprocessedTagsUI : UserControl
  {

    ObservableCollection<UnprocessedTagForSegment> _data = null;
    public UnprocessedTagsUI()
    {
      this.InitializeComponent();

      _data = new ObservableCollection<UnprocessedTagForSegment>();
      lvTags.ItemsSource = _data;
    }

    private IHLSController _controller;

    public IHLSController Controller
    {
      get { return _controller; }
      set
      {
        if (_controller != null && _controller.IsValid && _controller.Playlist != null)
        { 
          _controller.Playlist.SegmentSwitched -= Playlist_SegmentSwitched;
        }
        _controller = value;
        if (_controller.IsValid && _controller.Playlist != null)
        { 
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
      if (lvTags.Items.Count == 0)
      {
        notagsStatus.Visibility = Visibility.Collapsed;
        noplaybackStatus.Visibility = Visibility.Visible;
      }
      
    
    }

    void _player_MediaOpened(object sender, RoutedEventArgs e)
    {
      noplaybackStatus.Visibility = Visibility.Collapsed;
      notagsStatus.Visibility = Visibility.Visible;
    
    }


    void Playlist_SegmentSwitched(IHLSPlaylist sender, IHLSSegmentSwitchEventArgs args)
    {
      Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
      {
        try
        {
          var pretaglist = args.ToSegment.GetUnprocessedTags(UnprocessedTagPlacement.PRE);
          var withintaglist = args.ToSegment.GetUnprocessedTags(UnprocessedTagPlacement.WITHIN);
          UnprocessedTagForSegment tagplacement = new UnprocessedTagForSegment() { ForSegmentSequence = args.ToSegment.SequenceNumber };
          if (pretaglist != null && pretaglist.Count > 0)
            tagplacement.Tags.AddRange(pretaglist);
          if (withintaglist != null && withintaglist.Count > 0)
            tagplacement.Tags.AddRange(withintaglist);

          if (tagplacement.Tags.Count > 0)
            _data.Add(tagplacement);

          if (_data.Count > 0 && notagsStatus.Visibility == Visibility.Visible)
            notagsStatus.Visibility = Visibility.Collapsed;
        }
        catch(Exception Ex)
        {

        }

      });
    }


  }
}
