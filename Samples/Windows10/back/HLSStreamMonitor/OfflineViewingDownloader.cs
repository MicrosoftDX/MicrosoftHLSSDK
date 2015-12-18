using Microsoft.HLSClient;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Json;
using System.Threading.Tasks;
using Windows.Storage;
using Windows.Storage.Streams;
using Windows.Web.Http;

namespace HLSStreamMonitor
{
   

  public class ContentDownloadErrorArgs : IHLSContentDownloadErrorArgs
  {
    public ContentDownloadErrorArgs(HttpStatusCode code)
    {
      _statusCode = code;
    }
    HttpStatusCode _statusCode;
    public HttpStatusCode StatusCode
    {
      get { return _statusCode; }
    }
  }

 

  public class ContentDownloadCompletedArgs : IHLSContentDownloadCompletedArgs
  {
    public ContentDownloadCompletedArgs(string Uri)
    {
      _contentUri = new Uri(Uri);
    }
    public void SetData(IBuffer content, HttpStatusCode StatusCode)
    {
      _content = content;
      _statusCode = StatusCode;
    }
    private IBuffer _content = null;
    public IBuffer Content
    {
      get { return _content; }
    }

    private Uri _contentUri = default(Uri);
    public Uri ContentUri
    {
      get { return _contentUri; }
    }


    public bool IsSuccessStatusCode
    {
      get { return _statusCode == HttpStatusCode.Ok; }
    }

    public IReadOnlyDictionary<string, string> ResponseHeaders
    {
      get { return null; }
    }

    HttpStatusCode _statusCode = HttpStatusCode.Ok;
    public Windows.Web.Http.HttpStatusCode StatusCode
    {
      get { return _statusCode; }
    }
  }
   
  
  class OfflineViewingDownloader : Microsoft.HLSClient.IHLSContentDownloader
  {

    bool _isBusy; 
    string _contentUri = null;

    public event Windows.Foundation.TypedEventHandler<Microsoft.HLSClient.IHLSContentDownloader, Microsoft.HLSClient.IHLSContentDownloadCompletedArgs> Completed;
    public event Windows.Foundation.TypedEventHandler<Microsoft.HLSClient.IHLSContentDownloader, Microsoft.HLSClient.IHLSContentDownloadErrorArgs> Error;
 
     
 
    public OfflineViewingDownloader()
    {
    }

    public void Cancel()
    {
      return;
    }



    public Windows.Foundation.IAsyncAction DownloadAsync()
    {

      return AsyncInfo.Run(async (ct) =>
        {
          ContentDownloadCompletedArgs args = new ContentDownloadCompletedArgs(_contentUri);
         

          if (args.Content == null)
          {
            HttpClient client = new HttpClient();
            HttpResponseMessage resp = await client.GetAsync(args.ContentUri);
            if (resp.IsSuccessStatusCode)
            {

              args.SetData(await resp.Content.ReadAsBufferAsync(), resp.StatusCode);
              if (this.Completed != null)
                this.Completed(this, args);
             
            }
            else
            {
              args.SetData(null, resp.StatusCode);
              if (this.Error != null)
                this.Error(this, new ContentDownloadErrorArgs(resp.StatusCode));
            }

          }
          else
          {
            if (this.Completed != null)
              this.Completed(this, args);
          }
        });

    }



    public void Initialize(string ContentUri)
    {
      _contentUri = ContentUri;
    }

    public bool IsBusy
    {
      get
      {
        return _isBusy;
      }
    }

    public bool SupportsCancellation
    {
      get { return false; }
    }
  }

  class CustomDownloader : Microsoft.HLSClient.IHLSContentDownloader
  {
    string _contentUri = null;
    bool _isBusy = false;
    public void Cancel()
    {
      return;
    }

    public event Windows.Foundation.TypedEventHandler<IHLSContentDownloader, IHLSContentDownloadCompletedArgs> Completed;

    public Windows.Foundation.IAsyncAction DownloadAsync()
    {
      _isBusy = true;
      return AsyncInfo.Run(async (ct) =>
      {
        ContentDownloadCompletedArgs args = new ContentDownloadCompletedArgs(_contentUri);

        HttpClient client = new HttpClient();
        HttpResponseMessage resp = await client.GetAsync(args.ContentUri);
        if (resp.IsSuccessStatusCode)
        {

          args.SetData(await resp.Content.ReadAsBufferAsync(), resp.StatusCode);
          if (this.Completed != null)
            this.Completed(this, args);

        }
        else
        {
          args.SetData(null, resp.StatusCode);
          if (this.Error != null)
            this.Error(this, new ContentDownloadErrorArgs(resp.StatusCode));
        }
        _isBusy = false;

      });
    }

    public event Windows.Foundation.TypedEventHandler<IHLSContentDownloader, IHLSContentDownloadErrorArgs> Error;

    public void Initialize(string ContentUri)
    {
      _contentUri = ContentUri;
    }

    public bool IsBusy
    {
      get { return _isBusy; }
    }

    public bool SupportsCancellation
    {
      get { return false; }
    }
  }
}
