using Microsoft.HLSClient;
using Microsoft.PlayerFramework.Adaptive.HLS;
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

// The Blank Page item template is documented at http://go.microsoft.com/fwlink/?LinkId=402352&clcid=0x409

namespace MSHLS.PF.XAML.Sample
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

		public MainPage()
        {
            this.InitializeComponent();
			this._StatsTimer = new DispatcherTimer() { Interval = TimeSpan.FromSeconds(5) };
			this._StatsTimer.Tick += StatsTimer_Tick;

			var plugin = this.Player.Plugins.FirstOrDefault(p => typeof(HLSPlugin) == p.GetType());
			if (null != plugin)
			{
				this._HLSPlugin = plugin as HLSPlugin;
				this.WireHLSPluginHandlers();
			}
		}

		protected async override void OnNavigatedTo(NavigationEventArgs e)
		{
			this.PlaylistsListView.ItemsSource = await PlaylistDataSource.Current.GetPlaylistsAsync();
		}

		protected override void OnNavigatedFrom(NavigationEventArgs e)
		{
			this._StatsTimer.Stop();
			this._StatsTimer.Tick -= StatsTimer_Tick;
			this.UnwireHLSPluginHandlers();
			this.Player.Dispose();
		}

		private void WireHLSPluginHandlers()
		{
			this._HLSPlugin.HLSControllerReady += HLSPlugin_HLSControllerReady;

            var plugin = Player.Plugins.FirstOrDefault(p => p.GetType() == typeof(Microsoft.PlayerFramework.CC608.CC608Plugin));
            if(plugin != null)
                (plugin as Microsoft.PlayerFramework.CC608.CC608Plugin).OnCaptionAdded += MainPage_OnCaptionAdded;
        }

        private void MainPage_OnCaptionAdded(object sender, Microsoft.PlayerFramework.CC608.UIElementEventArgs e)
        {
            var x = e.UIElement;
            var y = x;
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
	}
}
