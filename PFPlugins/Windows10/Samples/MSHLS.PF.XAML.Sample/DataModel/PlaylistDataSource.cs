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
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Threading.Tasks;
using Windows.Data.Json;
using Windows.Storage;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Media.Imaging;

// The data model defined by this file serves as a representative example of a strongly-typed
// model.  The property names chosen coincide with data bindings in the standard item templates.
//
// Applications may use this model as a starting point and build on it, or discard it entirely and
// replace it with something appropriate to their needs. If using this model, you might improve app 
// responsiveness by initiating the data loading task in the code behind for App.xaml when the app 
// is first launched.

namespace MSHLS.PF.XAML.Sample
{
  /// <summary>
  /// Generic item data model.
  /// </summary>
  public class Playlist
  {
    public Playlist(String title, String description, Uri playlistUri)
    {
      this.PlaylistId = Guid.NewGuid();
      this.Title = title;
      this.Description = description;
      this.PlaylistUri = playlistUri;
    }

    public Guid PlaylistId { get; private set; }
    public string Title { get; private set; }
    public string Description { get; private set; }
    public Uri PlaylistUri { get; private set; }

    public override string ToString()
    {
      return this.Title;
    }
  }

  /// <summary>
  /// Creates a collection of groups and items with content read from a static json file.
  /// 
  /// SampleDataSource initializes with data read from a static json file included in the 
  /// project.  This provides sample data at both design-time and run-time.
  /// </summary>
  public sealed class PlaylistDataSource
  {
    private static PlaylistDataSource _Current;
    public static PlaylistDataSource Current { get { if(null == _Current) _Current = new PlaylistDataSource(); return _Current; } }

    private ObservableCollection<Playlist> _Playlists;

    public async Task<IEnumerable<Playlist>> GetPlaylistsAsync()
    {
      if(null == this._Playlists)
      {
        this._Playlists = new ObservableCollection<Playlist>();
        Uri dataUri = new Uri("ms-appx:///DataModel/Playlists.json");

        StorageFile file = await StorageFile.GetFileFromApplicationUriAsync(dataUri);
        string jsonText = await FileIO.ReadTextAsync(file);
        JsonObject jsonObject = JsonObject.Parse(jsonText);
        JsonArray jsonArray = jsonObject["Playlists"].GetArray();

        foreach (JsonValue playlistValue in jsonArray)
        {
          JsonObject playlistObject = playlistValue.GetObject();
          this._Playlists.Add(
            new Playlist(playlistObject["Title"].GetString(),
              playlistObject["Description"].GetString(),
              new Uri(playlistObject["PlaylistUri"].GetString())));
        }
      }
      return this._Playlists;
    }

    public async Task<Playlist> GetPlaylistAsync(Guid playlistId)
    {
      return (await this.GetPlaylistsAsync()).FirstOrDefault(p => playlistId == p.PlaylistId);
    }
  }
}