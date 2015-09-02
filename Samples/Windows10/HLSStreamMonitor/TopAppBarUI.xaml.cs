using Microsoft.PlayerFramework;
using System;
using System.Collections.Generic;
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
  public sealed partial class TopAppBarUI : UserControl
  {

    public event EventHandler<Uri> PlayRequested;
    public event EventHandler<List<Uri>> BatchPlayRequested;
    public event EventHandler PlaylistRequested;
    public TopAppBarUI()
    {
      this.InitializeComponent();
    }

    private MediaPlayer _player;

    public MediaPlayer Player
    {
      get { return _player; }
      set { _player = value; }
    }

    private void btnPlay_Tapped(object sender, TappedRoutedEventArgs e)
    {

      if (Player.GetPlaylistPlugin().Playlist.Count > 0 && PlaylistRequested != null)
      {
        PlaylistRequested(this, null);
        return;
      }

      string entry = tbxURL.Text;
      if (entry == null || entry == String.Empty)
        return;
      if (entry.Contains(System.Environment.NewLine))
      {
        var batch = entry.Split(new string[] { System.Environment.NewLine }, StringSplitOptions.RemoveEmptyEntries);
        List<Uri> ret = new List<Uri>();
        foreach (string u in batch)
        {
          Uri uri = null;

          try
          {
            uri = new Uri(u, UriKind.Absolute);
          }
          catch
          {
            return;
          }

          if (uri != null)
            ret.Add(uri);
        }
        if (BatchPlayRequested != null)
          BatchPlayRequested(this, ret);
      }
      else
      {

        Uri ret = null;

        try
        {
          ret = new Uri(entry, UriKind.Absolute);
        }
        catch
        {
          return;
        }

        if (ret != null && PlayRequested != null)
          PlayRequested(this, ret);
      }

    }

    private void btnAddToFav_Tapped(object sender, TappedRoutedEventArgs e)
    {
      ((FlyoutBase.GetAttachedFlyout(btnAddToFav) as Flyout).Content as FavoritesUI).IsAddMode = true;
      ((FlyoutBase.GetAttachedFlyout(btnAddToFav) as Flyout).Content as FavoritesUI).Url = tbxURL.Text;
      ((FlyoutBase.GetAttachedFlyout(btnAddToFav) as Flyout).Content as FavoritesUI).Container = FlyoutBase.GetAttachedFlyout(btnAddToFav) as Flyout;
      FlyoutBase.ShowAttachedFlyout((FrameworkElement)sender);
    }

    private void btnShowFavs_Tapped(object sender, TappedRoutedEventArgs e)
    {
      ((FlyoutBase.GetAttachedFlyout(btnShowFavs) as Flyout).Content as FavoritesUI).Player = Player;
      ((FlyoutBase.GetAttachedFlyout(btnShowFavs) as Flyout).Content as FavoritesUI).IsAddMode = false;
      ((FlyoutBase.GetAttachedFlyout(btnShowFavs) as Flyout).Content as FavoritesUI).Container = FlyoutBase.GetAttachedFlyout(btnShowFavs) as Flyout;

      (FlyoutBase.GetAttachedFlyout(btnShowFavs) as Flyout).Closed += (s, args) =>
        {
          if (((FlyoutBase.GetAttachedFlyout(btnShowFavs) as Flyout).Content as FavoritesUI).Url != null)
          {
            tbxURL.Text = ((FlyoutBase.GetAttachedFlyout(btnShowFavs) as Flyout).Content as FavoritesUI).Url;
          }
          btnPlay_Tapped(this, null);

        };
      FlyoutBase.ShowAttachedFlyout((FrameworkElement)sender);
    }


    private void tbxURL_TextChanged(object sender, TextChangedEventArgs e)
    {
      btnAddToFav.IsEnabled = Uri.IsWellFormedUriString(tbxURL.Text, UriKind.Absolute);

    }
  }
}
