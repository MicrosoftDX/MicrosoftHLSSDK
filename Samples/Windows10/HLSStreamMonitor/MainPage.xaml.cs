
using Microsoft.HLSClient;
using Microsoft.PlayerFramework;
using Microsoft.PlayerFramework.Adaptive.HLS;
using System;
using System.Collections.Generic;
using System.Linq;
using Windows.Foundation;
using Windows.UI.Core;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Media.Imaging;
using Windows.UI.Xaml.Navigation;

// The Blank Page item template is documented at http://go.microsoft.com/fwlink/?LinkId=234238

namespace HLSStreamMonitor
{
  /// <summary>
  /// An empty page that can be used on its own or navigated to within a Frame.
  /// </summary>
  public sealed partial class MainPage : Page
  {
    //the controller factory
    IHLSControllerFactory _controllerfactory = null;
    //the controller
    IHLSController _controller = null;

    List<Uri> _batch = null;
    public MainPage()
    {
      this.InitializeComponent();
      ucTopAppBar.Player = mediaplayer;
      ucTopAppBar.PlayRequested += ucTopAppBar_PlayRequested;
      ucTopAppBar.BatchPlayRequested += ucTopAppBar_BatchPlayRequested;
      ucTopAppBar.PlaylistRequested += ucTopAppBar_PlaylistRequested;

      mediaplayer.GetPlaylistPlugin().SkippingNext += MainPage_SkippingNext;
      mediaplayer.GetPlaylistPlugin().SkippingPrevious += MainPage_SkippingPrevious;
      mediaplayer.GetPlaylistPlugin().CurrentPlaylistItemChanged += MainPage_CurrentPlaylistItemChanged;
    }


    void ucTopAppBar_PlaylistRequested(object sender, EventArgs e)
    {
      this.TopAppBar.IsOpen = false;
      ucPlaybackStatus.Reset();
      mediaplayer.IsSkipNextVisible = true;
      mediaplayer.IsSkipPreviousVisible = true;

      mediaplayer.GetPlaylistPlugin().GoToNextPlaylistItem();
    }

    void MainPage_CurrentPlaylistItemChanged(object sender, EventArgs e)
    {
      ucPlaybackStatus.Reset();
    }

    void MainPage_SkippingPrevious(object sender, SkipToPlaylistItemEventArgs e)
    {
      ucPlaybackStatus.Reset();
    }

    void MainPage_SkippingNext(object sender, SkipToPlaylistItemEventArgs e)
    {
      ucPlaybackStatus.Reset();
    }

    void ucTopAppBar_BatchPlayRequested(object sender, List<Uri> e)
    {
      this.TopAppBar.IsOpen = false;
      ucPlaybackStatus.Reset();
      mediaplayer.IsSkipNextVisible = false;
      mediaplayer.IsSkipPreviousVisible = false;
      mediaplayer.Source = e.First();
      _batch = e.Skip(1).ToList();
    }

    void ucTopAppBar_PlayRequested(object sender, Uri e)
    {
      this.TopAppBar.IsOpen = false;
      ucPlaybackStatus.Reset();
      mediaplayer.IsSkipNextVisible = false;
      mediaplayer.IsSkipPreviousVisible = false;
      mediaplayer.Source = e;
    }

    protected override void OnNavigatedTo(NavigationEventArgs e)
    {
      base.OnNavigatedTo(e);

      //get the HLS plugin from the PF player
      var hlsplugin = mediaplayer.Plugins.Where(p => p is HLSPlugin).FirstOrDefault();
      //get the controller factory
      _controllerfactory = (hlsplugin as HLSPlugin).ControllerFactory;
      //subscribe to HLSControllerReady
      _controllerfactory.HLSControllerReady += _controllerfactory_HLSControllerReady;
      //do this only if you need to modify URL or headers or cookies
      //_controllerfactory.PrepareResourceRequest += _controllerfactory_PrepareResourceRequest;
      ucHLSSettings.Player = mediaplayer;
      ucMetadataUI.Player = mediaplayer;
      ucUnprocessedTagsUI.Player = mediaplayer;
      this.TopAppBar.IsOpen = true;

    }

    void _controllerfactory_PrepareResourceRequest(IHLSControllerFactory sender, IHLSResourceRequestEventArgs args)
    {
      //add code to modify initial root playlist HTTP request if needed
      args.Submit();
    }

    void _controller_PrepareResourceRequest(IHLSController sender, IHLSResourceRequestEventArgs args)
    {
      //var downloader = new CustomDownloader(); 
      //args.SetDownloader(downloader);
      args.Submit();
    }
    void _controllerfactory_HLSControllerReady(IHLSControllerFactory sender, IHLSController args)
    {
     
      //save the controller
      _controller = args;

      //playing a batch ?
      if (_batch != null)
      {
        _controller.BatchPlaylists(_batch.Select(u =>
        {
          return u.AbsoluteUri;
        }).ToList());
      }
      
      if (_controller.Playlist != null)
      {
        _controller.Playlist.StreamSelectionChanged += Playlist_StreamSelectionChanged;
        _controller.Playlist.SegmentSwitched += Playlist_SegmentSwitched;
      }

      _controller.PrepareResourceRequest += _controller_PrepareResourceRequest;
      _controller.InitialBitrateSelected += _controller_InitialBitrateSelected;

      Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
      {
        gridAudioOnly.Visibility = Visibility.Collapsed;
        try
        {
          if (_controller.IsValid)
          {
            ucPlaybackStatus.Initialize(_controller, mediaplayer);
            ucHLSSettings.Controller = _controller;
            ucMetadataUI.Controller = _controller;
            ucUnprocessedTagsUI.Controller = _controller;
          }
        }
        catch (Exception Ex)
        {

        }
      }).AsTask().Wait();

    }

    void Playlist_SegmentSwitched(IHLSPlaylist sender, IHLSSegmentSwitchEventArgs args)
    {
      Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
     {
       var hasme = VisualTreeHelper.FindElementsInHostCoordinates(new Point(gridMediaDisplay.ActualWidth / 2, gridMediaDisplay.ActualHeight / 2), gridMediaDisplay).
                                                    Where(uie => uie is MediaElement).FirstOrDefault();
       if (args.ToSegment.MediaType == TrackType.AUDIO && hasme != null) //first segment being played and audio only
         SwitchTrackTypeVisual(TrackType.AUDIO);
     });

    }

    void _controller_InitialBitrateSelected(IHLSController sender, IHLSInitialBitrateSelectedEventArgs args)
    {
      //use this to set any propeties on the initial ActiveVariant before playback starts

      //E.g. set an alternate audio track to start with
      //if (_controller != null && _controller.Playlist.IsMaster)
      //{

      //  var audiorenditions = _controller.Playlist.ActiveVariantStream.GetAudioRenditions();
      //  if (audiorenditions != null)
      //  {
      //    var match = audiorenditions.ToList().Find((r) =>
      //    {
      //      return r.Name == "someotheralternateaudio";
      //    });
      //    if (match != null)
      //      match.IsActive = true;
      //  }
      //}
      args.Submit();
    }

    void SwitchTrackTypeVisual(TrackType To)
    {
      try
      {
     
        MediaElement me = null;
        var hasme = VisualTreeHelper.FindElementsInHostCoordinates(new Point(gridMediaDisplay.ActualWidth / 2, gridMediaDisplay.ActualHeight / 2), gridMediaDisplay).
                                                    Where(uie => uie is MediaElement).FirstOrDefault();
        if (hasme != null)
        {
          me = hasme as MediaElement;
          if (me.Parent is Panel && To == TrackType.AUDIO &&
            (me.Parent as Panel).Children.Count() > (me.Parent as Panel).Children.IndexOf(me) + 1 &&
            (me.Parent as Panel).Children[(me.Parent as Panel).Children.IndexOf(me) + 1] == gridAudioOnly)
            return;
        }
        else
        {
          if (To == TrackType.AUDIO)
          {
            gridAudioOnly.Visibility = Visibility.Visible;
          }
          else
          {
            gridAudioOnly.Visibility = Visibility.Collapsed;
          }
        }
        //if we switched to an audio only stream - show poster
        if (To == TrackType.AUDIO)
        {
           
          if (me != null)
          {
           
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
        else if (To == TrackType.BOTH) //else hide poster if poster was being displayed
        {

        
          if (me != null)
          { 
            if (me.Parent is Panel)
              (me.Parent as Panel).Children.RemoveAt((me.Parent as Panel).Children.IndexOf(me) + 1);
          }
        }
      }
      catch (Exception Ex)
      {

      }
    }


    void Playlist_StreamSelectionChanged(IHLSPlaylist sender, IHLSStreamSelectionChangedEventArgs args)
    {
      Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () =>
      {
        SwitchTrackTypeVisual(args.To);

      });
    }

    private void btnToggleFullScreen_Checked(object sender, RoutedEventArgs e)
    {
      gridControlPanel.Visibility = Visibility.Collapsed;
      gridHLSData.Visibility = Visibility.Collapsed;
      Grid.SetRowSpan(gridMediaDisplay, 2);
      Grid.SetColumnSpan(gridMediaDisplay, 2);
    }

    private void btnToggleFullScreen_Unchecked(object sender, RoutedEventArgs e)
    {
      Grid.SetRowSpan(gridMediaDisplay, 1);
      Grid.SetColumnSpan(gridMediaDisplay, 1);
      gridControlPanel.Visibility = Visibility.Visible;
      gridHLSData.Visibility = Visibility.Visible;

    }
  }
}
