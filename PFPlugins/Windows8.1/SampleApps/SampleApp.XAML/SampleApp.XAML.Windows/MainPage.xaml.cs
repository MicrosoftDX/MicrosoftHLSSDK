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
using Microsoft.PlayerFramework.Adaptive.HLS;
using SampleApp.XAML.Common;
using SampleApp.XAML.Data;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading.Tasks;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;

// The Blank Page item template is documented at http://go.microsoft.com/fwlink/?LinkId=234238

namespace SampleApp.XAML
{
  /// <summary>
  /// An empty page that can be used on its own or navigated to within a Frame.
  /// </summary>
  public sealed partial class MainPage : Page
  {
    private HLSPlugin _HLSPlugin;
    private IHLSController _HLSController;
    private IHLSPlaylist _HLSPlaylist;
    private DispatcherTimer _StatsTimer;

    /// <summary>
    /// NavigationHelper is used on each page to aid in navigation and 
    /// process lifetime management
    /// </summary>
    public NavigationHelper NavigationHelper { get; private set; }

    private ObservableDictionary _DefaultViewModel = new ObservableDictionary();
    public ObservableDictionary DefaultViewModel { get { return this._DefaultViewModel; } }

    public MainPage()
    {
      this.InitializeComponent();
      this._StatsTimer = new DispatcherTimer() { Interval = TimeSpan.FromSeconds(5) };
      this._StatsTimer.Tick += StatsTimer_Tick;

      this.NavigationHelper = new NavigationHelper(this);
      this.NavigationHelper.LoadState += NavigationHelper_LoadState;

      var plugin = this.Player.Plugins.FirstOrDefault(p => typeof(HLSPlugin) == p.GetType());
      if (null != plugin)
      {
        this._HLSPlugin = plugin as HLSPlugin;
        this.WireHLSPluginHandlers();
      }
    }

    private async void NavigationHelper_LoadState(object sender, LoadStateEventArgs e)
    {
      var data = await PlaylistDataSource.Current.GetPlaylistsAsync();
      this.DefaultViewModel["Playlists"] = data;
    }

    private void WireHLSPluginHandlers() 
    {
      this._HLSPlugin.HLSControllerReady += HLSPlugin_HLSControllerReady;
    }

    private void UnwireHLSPluginHandlers()
    {
      this._HLSPlugin.HLSControllerReady -= HLSPlugin_HLSControllerReady;
      this.UnwireHLSPlaylistHandlers();
    }

    private void WireHLSPlaylistHandlers()
    {
      if (null != this._HLSPlaylist)
      {
        this._HLSPlaylist.BitrateSwitchSuggested += HLSPlaylist_BitrateSwitchSuggested;
        this._HLSPlaylist.BitrateSwitchCompleted += HLSPlaylist_BitrateSwitchCompleted;
      }
    }

    private void UnwireHLSPlaylistHandlers()
    {
      if (null != this._HLSPlaylist)
      {
        this._HLSPlaylist.BitrateSwitchSuggested -= HLSPlaylist_BitrateSwitchSuggested;
        this._HLSPlaylist.BitrateSwitchCompleted -= HLSPlaylist_BitrateSwitchCompleted;
      }
    }

    private void HLSPlugin_HLSControllerReady(object sender, IHLSController e)
    {
      if (null != e)
      {
        this._HLSController = e;

        if (null != this._HLSPlaylist)
          this.UnwireHLSPlaylistHandlers();

        if (null != e.Playlist)
        {
          this._HLSPlaylist = e.Playlist;
          this.WireHLSPlaylistHandlers();
        }
      }
    }

    private void HLSPlaylist_BitrateSwitchSuggested(IHLSPlaylist sender, IHLSBitrateSwitchEventArgs args)
    {
      this.WriteStatsMsgAsync(
        "HLSPlaylist_BitrateSwitchSuggested",
        "from bitrate: {0}, to bitrate: {1}", 
        args.FromBitrate, args.ToBitrate);
    }

    private void HLSPlaylist_BitrateSwitchCompleted(IHLSPlaylist sender, IHLSBitrateSwitchEventArgs args)
    {
      this.WriteStatsMsgAsync(
        "HLSPlaylist_BitrateSwitchCompleted",
        "from bitrate: {0}, to bitrate: {1}",
        args.FromBitrate, args.ToBitrate);
    }

    private void PlaylistUriBtn_Click(object sender, RoutedEventArgs e)
    {
      if (!string.IsNullOrWhiteSpace(this.PlaylistUriEntry.Text))
      {
        Uri playlistUri;
        if (Uri.TryCreate(this.PlaylistUriEntry.Text, UriKind.RelativeOrAbsolute, out playlistUri))
          this.SetPlayerSourceAsync(playlistUri);
      }
    }

    private void PlaylistsListView_ItemClick(object sender, ItemClickEventArgs e)
    {
      Playlist p = e.ClickedItem as Playlist;
      this.SetPlayerSourceAsync(p.PlaylistUri);
    }

    private async Task SetPlayerSourceAsync(Uri playlistUri)
    {
      await this.Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
      {
        if (null != playlistUri)
          this.Player.Source = playlistUri;

        this.TopAppBar.IsOpen = false;
      });
    }

    private async void StatsTimer_Tick(object sender, object e)
    {
      await this.Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
      {
        this.StatsPopup.IsOpen = false;
      });
    }

    private void StatsPopup_Opened(object sender, object e)
    {
      this._StatsTimer.Stop();
      this._StatsTimer.Start();
    }

    private void StatsPopup_Closed(object sender, object e)
    {
      this._StatsTimer.Stop();
    }

    private async Task WriteStatsMsgAsync(string title, string format, params object[] args)
    {
      await this.Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
      {
        if (!string.IsNullOrWhiteSpace(format))
        {
          this.StatsTitle.Text = title;
          this.StatsMsg.Text = string.Format(format, args);
          this.StatsPopup.IsOpen = true;
        }
        else
        {
          this.StatsPopup.IsOpen = false;
        }
      });
    }

    #region NavigationHelper registration

    /// The methods provided in this section are simply used to allow
    /// NavigationHelper to respond to the page's navigation methods.
    /// 
    /// Page specific logic should be placed in event handlers for the  
    /// <see cref="GridCS.Common.NavigationHelper.LoadState"/>
    /// and <see cref="GridCS.Common.NavigationHelper.SaveState"/>.
    /// The navigation parameter is available in the LoadState method 
    /// in addition to page state preserved during an earlier session.

    protected override void OnNavigatedTo(NavigationEventArgs e)
    {
      this.NavigationHelper.OnNavigatedTo(e);
    }

    protected override void OnNavigatedFrom(NavigationEventArgs e)
    {
      this.NavigationHelper.OnNavigatedFrom(e);
      this._StatsTimer.Stop();
      this._StatsTimer.Tick -= StatsTimer_Tick;
      this.UnwireHLSPluginHandlers();
      this.Player.Dispose();
    }

    #endregion
  }
}
