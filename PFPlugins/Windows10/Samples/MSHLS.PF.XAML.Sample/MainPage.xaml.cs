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
		public MainPage()
        {
            this.InitializeComponent();
		}

		protected async override void OnNavigatedTo(NavigationEventArgs e)
		{
			this.PlaylistsListView.ItemsSource = await PlaylistDataSource.Current.GetPlaylistsAsync();
        }

        private void PlaylistUriBtn_Click(object sender, RoutedEventArgs e)
        {
            if (!string.IsNullOrWhiteSpace(this.PlaylistUriEntry.Text))
            {
                Uri playlistUri;
                if (Uri.TryCreate(this.PlaylistUriEntry.Text, UriKind.RelativeOrAbsolute, out playlistUri))
                    this.Frame.Navigate(typeof(PlayerPage), playlistUri);
            }
        }

        private void PlaylistsListView_ItemClick(object sender, ItemClickEventArgs e)
        {
            this.Frame.Navigate(typeof(PlayerPage), (e.ClickedItem as Playlist).PlaylistUri);
        }
    }
}
