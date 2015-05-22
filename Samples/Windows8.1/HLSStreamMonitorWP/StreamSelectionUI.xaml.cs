using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;
using System.Xml.Linq;
using Windows.Storage;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Input;

// The User Control item template is documented at http://go.microsoft.com/fwlink/?LinkId=234236

namespace HLSStreamMonitorWP
{
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
  public sealed partial class StreamSelectionUI : UserControl, INotifyPropertyChanged
  {
    public event EventHandler<string> StreamSelected;

    public string Url { get; set; }
    private void OnPropertyChanged([CallerMemberName] string PropName = "")
    {
      if (PropertyChanged != null)
        PropertyChanged(this, new PropertyChangedEventArgs(PropName));
    }

    private bool _IsInSelectionMode = true;

    public bool IsInSelectionMode
    {
      get { return _IsInSelectionMode; }
      set { _IsInSelectionMode = value; OnPropertyChanged(); }
    }


    private ObservableCollection<Favorite> _favorites = new ObservableCollection<Favorite>();

    public ObservableCollection<Favorite> Favorites
    {
      get
      {

        return _favorites;
      }

    }
    public StreamSelectionUI()
    {
      this.InitializeComponent();
      this.Loaded += StreamSelectionUI_Loaded;
      this.DataContext = this;

    }

    async void StreamSelectionUI_Loaded(object sender, RoutedEventArgs e)
    {
      if (_favorites.Count == 0)
        await ReadFavorites();

      if (_favorites.Count > 0)
        _IsInSelectionMode = true;
      else
        _IsInSelectionMode = false;

    }


    private async Task ReadFavorites()
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
      if (ApplicationData.Current.LocalSettings.Containers.ContainsKey("Favorites") == false)
        return;
      else
      {

        favContainer = ApplicationData.Current.LocalSettings.Containers["Favorites"];
        foreach (var fav in favContainer.Values)
        {
          _favorites.Add(new Favorite() { Name = fav.Key, Url = fav.Value as string });
        }

      }
    }

    private void gvFavorites_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
      if (StreamSelected != null)
      {
        if (gvFavorites.SelectedIndex >= 0)
        {
          Url = (gvFavorites.SelectedItem as Favorite).Url;
          StreamSelected(this, (gvFavorites.SelectedItem as Favorite).Url);
        }
        else
        {
          Url = null;
          StreamSelected(this, null);
        }
      }
    }

    private void btnDone_Tapped(object sender, TappedRoutedEventArgs e)
    {
      if (IsInSelectionMode == false)
        IsInSelectionMode = true;
      else
      {
        if (StreamSelected != null)
        {
          StreamSelected(this, null);
        }
      }
    }

    public event PropertyChangedEventHandler PropertyChanged;

    private void btnSwitchToURLEntry_Click(object sender, RoutedEventArgs e)
    {
      IsInSelectionMode = false;
    }

    private void btnGo_Click(object sender, RoutedEventArgs e)
    {
      if (StreamSelected != null)
      {
        if (String.IsNullOrEmpty(tbxUrl.Text) == false)
        {
          Url = tbxUrl.Text;
          StreamSelected(this, tbxUrl.Text);
        }
        else
        {
          Url = null;
          StreamSelected(this, null);
        }
      }
    }

    private void btnSaveAndGo_Click(object sender, RoutedEventArgs e)
    {

      if (StreamSelected != null)
      {
        if (String.IsNullOrEmpty(tbxUrl.Text) || String.IsNullOrEmpty(tbxFavName.Text))
          StreamSelected(this, null);

        ApplicationDataContainer favContainer = null;
        if (ApplicationData.Current.LocalSettings.Containers.ContainsKey("Favorites"))
          favContainer = ApplicationData.Current.LocalSettings.Containers["Favorites"];
        else
          favContainer = ApplicationData.Current.LocalSettings.CreateContainer("Favorites", ApplicationDataCreateDisposition.Always);

        if (favContainer.Values.ContainsKey(tbxFavName.Text))
          favContainer.Values[tbxFavName.Text] = tbxUrl.Text;
        else
          favContainer.Values.Add(tbxFavName.Text, tbxUrl.Text);
        Url = tbxUrl.Text;
        StreamSelected(this, tbxUrl.Text);
      }
      else
      {
        Url = null;
        StreamSelected(this, null);
      }
    }

    
  }
}
