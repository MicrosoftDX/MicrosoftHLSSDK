using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.Storage;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;
using System.Xml.Linq;
using Microsoft.PlayerFramework;


// The User Control item template is documented at http://go.microsoft.com/fwlink/?LinkId=234236

namespace HLSStreamMonitor
{
  public sealed partial class FavoritesUI : UserControl, INotifyPropertyChanged
  {

    private bool _isAddMode = false;

    public bool IsAddMode
    {
      get { return _isAddMode; }
      set
      {
        _isAddMode = value;
        if (_isAddMode == false)
          ReadFavorites();
        if (PropertyChanged != null)
          PropertyChanged(this, new PropertyChangedEventArgs("IsAddMode"));
      }
    }

    private string _url;

    public string Url
    {
      get { return _url; }
      set
      {
        _url = value;
        if (PropertyChanged != null)
          PropertyChanged(this, new PropertyChangedEventArgs("Url"));
      }
    }

    private Flyout _container;

    public Flyout Container
    {
      get { return _container; }
      set { _container = value; }
    }


    private MediaPlayer _player;

    public MediaPlayer Player
    {
      get { return _player; }
      set { _player = value; }
    }

    private ObservableCollection<Favorite> _favorites = new ObservableCollection<Favorite>();

    public ObservableCollection<Favorite> Favorites
    {
      get { return _favorites; }

    }

    public FavoritesUI()
    {
      this.InitializeComponent();
      this.DataContext = this;
    }



    private async void ReadFavorites()
    {
      _favorites.Clear();
      try
      {
        var file = await Windows.Storage.StorageFile.GetFileFromApplicationUriAsync(new Uri("ms-appx:///assets/PermanentFavorites.xml"));
        var stream = await file.OpenStreamForReadAsync();
        XDocument xDoc = XDocument.Load(stream);
        foreach (XElement xelemFav in xDoc.Root.Elements(XName.Get("Favorite")))
        {
          _favorites.Add(new Favorite() { Name = xelemFav.Attribute(XName.Get("Name")).Value, Url = xelemFav.Attribute(XName.Get("Url")).Value, CanRemove = false });
        }
      }
      catch (Exception ex)
      {
        return;
      }

      ApplicationDataContainer favContainer = null;
      if (ApplicationData.Current.LocalSettings.Containers.ContainsKey("Favorites") == true)
      {

        favContainer = ApplicationData.Current.LocalSettings.Containers["Favorites"];
        foreach (var fav in favContainer.Values)
        {
          _favorites.Add(new Favorite() { Name = fav.Key, Url = fav.Value as string });
        }

      }



      if (PropertyChanged != null)
        PropertyChanged(this, new PropertyChangedEventArgs("Favorites"));
    }
    private void btnOkAdd_Tapped(object sender, TappedRoutedEventArgs e)
    {
      if (tbxFavName.Text == "")
        return;

      ApplicationDataContainer favContainer = null;
      if (ApplicationData.Current.LocalSettings.Containers.ContainsKey("Favorites"))
        favContainer = ApplicationData.Current.LocalSettings.Containers["Favorites"];
      else
        favContainer = ApplicationData.Current.LocalSettings.CreateContainer("Favorites", ApplicationDataCreateDisposition.Always);

      if (favContainer.Values.ContainsKey(tbxFavName.Text))
        favContainer.Values[tbxFavName.Text] = Url;
      else
        favContainer.Values.Add(tbxFavName.Text, Url);

      Container.Hide();
    }

    private void btnCancelAdd_Tapped(object sender, TappedRoutedEventArgs e)
    {
      Container.Hide();
    }

    private void btnOkSelect_Tapped(object sender, TappedRoutedEventArgs e)
    {
      _url = (gvFavorites.SelectedItem as Favorite).Url;
      Container.Hide();
    }

    private void btnCancelSelect_Tapped(object sender, TappedRoutedEventArgs e)
    {
      _url = null;
      Container.Hide();
    }

    private void btnPlaylist_Tapped(object sender, TappedRoutedEventArgs e)
    {
      var plplugin = Player.GetPlaylistPlugin();
      foreach (Favorite fav in gvFavorites.SelectedItems)
      {
        plplugin.Playlist.Add(new PlaylistItem() { SourceUri = fav.Url, AutoPlay = true, AutoLoad = true });
      }

      Container.Hide();
    }

    private void btnRemoveSelect_Tapped(object sender, TappedRoutedEventArgs e)
    {
      if (this.gvFavorites.SelectedIndex >= 0)
      {
        ApplicationDataContainer favContainer = null;
        if (ApplicationData.Current.LocalSettings.Containers.ContainsKey("Favorites"))
          favContainer = ApplicationData.Current.LocalSettings.Containers["Favorites"];
        else
          return;

        if (favContainer.Values.ContainsKey((this.gvFavorites.SelectedItem as Favorite).Name))
        {
          favContainer.Values.Remove((this.gvFavorites.SelectedItem as Favorite).Name);
          _favorites.Remove((this.gvFavorites.SelectedItem as Favorite));
          if (PropertyChanged != null)
            PropertyChanged(this, new PropertyChangedEventArgs("Favorites"));
        }
      }
    }

    public event PropertyChangedEventHandler PropertyChanged;

    private void gvFavorites_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
      if (gvFavorites.SelectedItems.Count > 1)
      {
        btnPlaylist.IsEnabled = true;
        btnRemoveSelect.IsEnabled = false;
        btnOkSelect.IsEnabled = false;
      }
      else
      {
        btnPlaylist.IsEnabled = false;
        if (gvFavorites.SelectedIndex >= 0)
        {
          btnOkSelect.IsEnabled = true;
          btnRemoveSelect.IsEnabled = (gvFavorites.SelectedItem as Favorite).CanRemove; 
        }
        else
        {
          btnRemoveSelect.IsEnabled = false;
          btnOkSelect.IsEnabled = false;
        }
      }
    }

    private void chkbxSelectAll_Checked(object sender, RoutedEventArgs e)
    {
      gvFavorites.SelectAll();
    }

    private void chkbxSelectAll_Unchecked(object sender, RoutedEventArgs e)
    {
      gvFavorites.SelectedItems.Clear();
    }



  }

  public class Favorite
  {
    public string Name { get; set; }
    public string Url { get; set; }

    private bool _canRemove = true;

    public bool CanRemove
    {
      get { return _canRemove; }
      set { _canRemove = value; }
    }

  }
}
