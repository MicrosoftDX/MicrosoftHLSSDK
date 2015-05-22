using Microsoft.HLSClient;
using Microsoft.PlayerFramework;
using Microsoft.PlayerFramework.Adaptive.HLS;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
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
  public sealed partial class HLSSettingsUI : UserControl
  {
    HLSSettings _data = null;
    HLSPlugin _hlsplugin = null;
    public HLSSettingsUI()
    {
      this.InitializeComponent();
      this.Loaded += HLSSettingsUI_Loaded;
    }

    void HLSSettingsUI_Loaded(object sender, RoutedEventArgs e)
    {
      if (_data == null)
        _data = new HLSSettings();

      this.DataContext = _data;

      _data.PropertyChanged += _data_PropertyChanged;

      this.cbxRates.ItemsSource = new List<RateDefinition>(){
        new RateDefinition(){Description = "12x Reverse",Value=-12.0d},
        new RateDefinition(){Description = "8x Reverse",Value=-8.0d},      
        new RateDefinition(){Description = "4x Reverse",Value=-4.0d},
        new RateDefinition(){Description = "2x Reverse",Value=-2.0d},       
        new RateDefinition(){Description = "Zero",Value=0.0d},        
        new RateDefinition(){Description = "Half speed Forward",Value=0.5d},
        new RateDefinition(){Description = "Normal",Value=1.0d},
        new RateDefinition(){Description = "2x Forward",Value=2.0d},
        new RateDefinition(){Description = "4x Forward",Value=4.0d},
        new RateDefinition(){Description = "8x Forward",Value=8.0d},
        new RateDefinition(){Description = "12x Forward",Value=12.0d},
      };

      this.cbxRates.SelectedIndex = 6;

      this.cbxMatchSegmentsUsing.ItemsSource = new List<SegmentMatchingCriterionDefinition>(){
        new SegmentMatchingCriterionDefinition(){Value = SegmentMatchCriterion.SEQUENCENUMBER},
        new SegmentMatchingCriterionDefinition(){Value = SegmentMatchCriterion.PROGRAMDATETIME}, 
      };

      this.cbxMatchSegmentsUsing.SelectedIndex = 0;

      this.cbxCCType.ItemsSource = new List<CCTypeDefinition>(){
        new CCTypeDefinition(){Description = "608",Value=ClosedCaptionType.CC608Instream},
        new CCTypeDefinition(){Description = "WebVTT",Value=ClosedCaptionType.WebVTTSidecar},      
        new CCTypeDefinition(){Description = "None",Value=ClosedCaptionType.None}
     
      };

      this.cbxCCType.SelectedIndex = 1;

      cbxRates.SelectionChanged += cbxRates_SelectionChanged;
      cbxMatchSegmentsUsing.SelectionChanged += cbxMatchSegmentsUsing_SelectionChanged;
      cbxAvailableBitrates.SelectionChanged += cbxAvailableBitrates_SelectionChanged;
      cbxCCType.SelectionChanged += cbxCCType_SelectionChanged;

    }

    void cbxCCType_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
      //if (_hlsplugin != null && cbxCCType.SelectedItem != null)
      //  _hlsplugin.ClosedCaptionType = ((CCTypeDefinition)cbxCCType.SelectedValue).Value;
    }

    void _data_PropertyChanged(object sender, PropertyChangedEventArgs e)
    {
      if (_controller != null && _controller.IsValid)
      {
        switch (e.PropertyName)
        {
          case "BitrateChangeNotificationInterval":
            if (_data.BitrateChangeNotificationInterval != default(TimeSpan))
              _controller.BitrateChangeNotificationInterval = _data.BitrateChangeNotificationInterval;
            break;
          case "MinimumBufferLength":
            if (_data.MinimumBufferLength != default(TimeSpan))
              _controller.MinimumBufferLength = _data.MinimumBufferLength;
            break;
          case "MinimumLiveLatency":
            if (_data.MinimumLiveLatency != default(TimeSpan))
              _controller.MinimumLiveLatency = _data.MinimumLiveLatency;
            break;
          case "PrefetchDuration":
            if (_data.PrefetchDuration != default(TimeSpan))
              _controller.PrefetchDuration = _data.PrefetchDuration;
            break;
          case "ForceAudioOnly":
            if (_data.ForceAudioOnly)
              _controller.TrackTypeFilter = TrackType.AUDIO;
            else
              _controller.TrackTypeFilter = TrackType.BOTH;
            break;
          case "AllowSegmentSkipOnSegmentFailure":
            _controller.AllowSegmentSkipOnSegmentFailure = _data.AllowSegmentSkipOnSegmentFailure;
            break;
          case "AutoAdjustScrubbingBitrate":
            _controller.AutoAdjustScrubbingBitrate = _data.AutoAdjustScrubbingBitrate;
            break;
          case "ResumeLiveFromPausedOrEarliest":
            _controller.ResumeLiveFromPausedOrEarliest = _data.ResumeLiveFromPausedOrEarliest;
            break;
          case "AutoAdjustTrickPlayBitrate":
            _controller.AutoAdjustTrickPlayBitrate = _data.AutoAdjustTrickPlayBitrate;
            break;
          case "UseTimeAveragedNetworkMeasure":
            _controller.UseTimeAveragedNetworkMeasure = _data.UseTimeAveragedNetworkMeasure;
            break;
          case "EnableAdaptiveBitrateMonitor":
            _controller.EnableAdaptiveBitrateMonitor = _data.EnableAdaptiveBitrateMonitor;
            break;
          case "UpshiftBitrateInSteps":
            _controller.UpshiftBitrateInSteps = _data.UpshiftBitrateInSteps;
            break;
          case "SegmentTryLimitOnBitrateSwitch":
            _controller.SegmentTryLimitOnBitrateSwitch = _data.SegmentTryLimitOnBitrateSwitch;
            break;
          case "MinimumPaddingForBitrateUpshift":
            _controller.MinimumPaddingForBitrateUpshift = _data.MinimumPaddingForBitrateUpshift;
            break;
          case "MaximumToleranceForBitrateDownshift":
            _controller.MaximumToleranceForBitrateDownshift = _data.MaximumToleranceForBitrateDownshift;
            break;
          case "StartBitrate":
            if (_controller.Playlist != null && _controller.Playlist.IsMaster &&
              _data.StartBitrate != 0 && _data.StartBitrate * 1024 <= _controller.Playlist.GetVariantStreams().Last().Bitrate &&
              _data.StartBitrate * 1024 >= _controller.Playlist.GetVariantStreams().First().Bitrate)

              _controller.Playlist.StartBitrate = _data.StartBitrate * 1024;
            break;
          case "MaximumBitrate":
            if (_controller.Playlist != null && _controller.Playlist.IsMaster &&
               _data.MaximumBitrate != 0 && _data.MaximumBitrate * 1024 >= _controller.Playlist.GetVariantStreams().First().Bitrate)

              _controller.Playlist.MaximumAllowedBitrate = _data.MaximumBitrate * 1024;
            break;
          case "MinimumBitrate":
            if (_controller.Playlist != null && _controller.Playlist.IsMaster &&
              _data.MinimumBitrate != 0 && _data.MinimumBitrate * 1024 <= _controller.Playlist.GetVariantStreams().Last().Bitrate)

              _controller.Playlist.MinimumAllowedBitrate = _data.MinimumBitrate * 1024;
            break;

        }

      }
    }

    void cbxMatchSegmentsUsing_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
      if (_controller != null && _controller.IsValid && _controller.Playlist != null && _controller.Playlist.IsMaster && cbxMatchSegmentsUsing.SelectedItem != null)
      {
        SegmentMatchingCriterionDefinition smc = cbxMatchSegmentsUsing.SelectedItem as SegmentMatchingCriterionDefinition;
        if (smc != null)
          _controller.MatchSegmentsUsing = smc.Value;
      }
    }

    void cbxRates_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
      if (cbxRates.SelectedItem == null || _player == null || _player.CurrentState != MediaElementState.Playing) return;

      _player.PlaybackRate = (cbxRates.SelectedItem as RateDefinition).Value;
    }

    void cbxAvailableBitrates_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
      if (_controller != null && _controller.IsValid && _controller.Playlist != null && _controller.Playlist.IsMaster && cbxAvailableBitrates.SelectedItem != null)
      {
        BitrateDefinition brd = cbxAvailableBitrates.SelectedItem as BitrateDefinition;
        if (!brd.Value.HasValue)
          _controller.Playlist.ResetBitrateLock();
        else
        {
          IHLSVariantStream variant = _controller.Playlist.GetVariantStreams().Where(vs => vs.Bitrate == brd.Value.Value).FirstOrDefault();
          if (variant != null)
            variant.LockBitrate();
        }
      }
    }

    private IHLSController _controller;

    public IHLSController Controller
    {
      get { return _controller; }
      set
      {
        _controller = value;

        if (_controller.IsValid && _controller.Playlist != null)
        {
          cbxAvailableBitrates.IsEnabled = _controller.Playlist.IsMaster;
          if (_controller.Playlist.IsMaster)
          {

            this.cbxAvailableBitrates.ItemsSource = new List<BitrateDefinition>() { new BitrateDefinition() { Value = null } }.
            Concat(_controller.Playlist.GetVariantStreams().Select((vs) => { return new BitrateDefinition() { Value = vs.Bitrate }; }));
            this.cbxAvailableBitrates.SelectedIndex = 0;
          }

          ApplyInitialHLSSettings();
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
          _player.MediaFailed -= _player_MediaFailed;
        }

        _player = value;
        _hlsplugin = _player.Plugins.FirstOrDefault(p => p is HLSPlugin) as HLSPlugin;
        
        _player.MediaOpened += _player_MediaOpened;
        _player.MediaEnded += _player_MediaEnded;
        _player.MediaFailed += _player_MediaFailed;
      }
    }

    void _player_MediaFailed(object sender, ExceptionRoutedEventArgs e)
    {

    }

    void _player_MediaEnded(object sender, MediaPlayerActionEventArgs e)
    {

    }

    void _player_MediaOpened(object sender, RoutedEventArgs e)
    {
      if (cbxRates.SelectedItem != null)
        _player.PlaybackRate = (cbxRates.SelectedItem as RateDefinition).Value;
    }



    private void ApplyInitialHLSSettings()
    {
      cbxCCType_SelectionChanged(null, null);

      if (_controller != null && _controller.IsValid)
      {
        if (_data.MinimumBufferLength != default(TimeSpan))
          _controller.MinimumBufferLength = _data.MinimumBufferLength;
        if (_data.PrefetchDuration != default(TimeSpan))
          _controller.PrefetchDuration = _data.PrefetchDuration;
        if (_data.MinimumLiveLatency != default(TimeSpan))
          _controller.MinimumLiveLatency = _data.MinimumLiveLatency;
        if (_data.BitrateChangeNotificationInterval != default(TimeSpan))
          _controller.BitrateChangeNotificationInterval = _data.BitrateChangeNotificationInterval;
        _controller.AllowSegmentSkipOnSegmentFailure = _data.AllowSegmentSkipOnSegmentFailure;
        _controller.AutoAdjustScrubbingBitrate = _data.AutoAdjustScrubbingBitrate;
        _controller.AutoAdjustTrickPlayBitrate = _data.AutoAdjustTrickPlayBitrate;
        _controller.UseTimeAveragedNetworkMeasure = _data.UseTimeAveragedNetworkMeasure;
        _controller.EnableAdaptiveBitrateMonitor = _data.EnableAdaptiveBitrateMonitor;
        _controller.UpshiftBitrateInSteps = _data.UpshiftBitrateInSteps;
        _controller.SegmentTryLimitOnBitrateSwitch = _data.SegmentTryLimitOnBitrateSwitch;
        _controller.MinimumPaddingForBitrateUpshift = _data.MinimumPaddingForBitrateUpshift;
        _controller.ResumeLiveFromPausedOrEarliest = _data.ResumeLiveFromPausedOrEarliest;
        if (_data.ForceAudioOnly)
          _controller.TrackTypeFilter = TrackType.AUDIO;
        else
          _controller.TrackTypeFilter = TrackType.BOTH;
        if (_controller.Playlist.IsMaster)
        {
          if (_data.StartBitrate != 0 && _data.StartBitrate * 1024 <= _controller.Playlist.GetVariantStreams().Last().Bitrate && _data.StartBitrate * 1024 >= _controller.Playlist.GetVariantStreams().First().Bitrate)
            _controller.Playlist.StartBitrate = _data.StartBitrate * 1024;
          if (_data.MaximumBitrate != 0 && _data.MaximumBitrate * 1024 >= _controller.Playlist.GetVariantStreams().First().Bitrate)
            _controller.Playlist.MaximumAllowedBitrate = _data.MaximumBitrate * 1024;
          if (_data.MinimumBitrate != 0 && _data.MinimumBitrate * 1024 <= _controller.Playlist.GetVariantStreams().Last().Bitrate)
            _controller.Playlist.MinimumAllowedBitrate = _data.MinimumBitrate * 1024;
        }
        if (cbxMatchSegmentsUsing.SelectedItem != null)
          _controller.MatchSegmentsUsing = (cbxMatchSegmentsUsing.SelectedItem as SegmentMatchingCriterionDefinition).Value;

      }
    }
  }


  public class HLSSettings : INotifyPropertyChanged
  {


    void OnPropertyChanged([CallerMemberName]string PropName = null)
    {
      if (PropertyChanged != null)
        PropertyChanged(this, new PropertyChangedEventArgs(PropName));

    }


    private uint? _LockedBitrate = null;

    public uint? LockedBitrate
    {
      get { return _LockedBitrate; }
      set { _LockedBitrate = value; }
    }


    private float _playbackRate = 1.0f;

    public float PlaybackRate
    {
      get { return _playbackRate; }
      set { _playbackRate = value; }
    }

    private TimeSpan _prefetchDuration = default(TimeSpan);

    public TimeSpan PrefetchDuration
    {
      get { return _prefetchDuration; }
      set
      {
        _prefetchDuration = value;
        OnPropertyChanged();
      }
    }

    private TimeSpan _minimumLiveLatency = TimeSpan.FromMilliseconds(30000);//default(TimeSpan);

    public TimeSpan MinimumLiveLatency
    {
      get { return _minimumLiveLatency; }
      set
      {
        _minimumLiveLatency = value;
        OnPropertyChanged();
      }
    }



    private TimeSpan _minimumBufferLength = TimeSpan.FromSeconds(75);

    public TimeSpan MinimumBufferLength
    {
      get { return _minimumBufferLength; }
      set
      {
        _minimumBufferLength = value;
        OnPropertyChanged();
      }
    }

    private TimeSpan _bitrateChangeNotificationInterval = TimeSpan.FromSeconds(15);

    public TimeSpan BitrateChangeNotificationInterval
    {
      get { return _bitrateChangeNotificationInterval; }
      set
      {
        _bitrateChangeNotificationInterval = value;
        OnPropertyChanged();
      }
    }

    private bool _enableAdaptiveBitrateMonitor = true;

    public bool EnableAdaptiveBitrateMonitor
    {
      get { return _enableAdaptiveBitrateMonitor; }
      set
      {
        _enableAdaptiveBitrateMonitor = value;
        OnPropertyChanged();
      }
    }

    private bool _forceAudioOnly = false;

    public bool ForceAudioOnly
    {
      get { return _forceAudioOnly; }
      set
      {
        _forceAudioOnly = value;
        OnPropertyChanged();
      }
    }

    private bool _resumeLiveFromPausedOrEarliest = false;

    public bool ResumeLiveFromPausedOrEarliest
    {
      get { return _resumeLiveFromPausedOrEarliest; }
      set
      {
        _resumeLiveFromPausedOrEarliest = value;
        OnPropertyChanged();
      }
    }

    private bool _useTimeAveragedNetworkMeasure = false;

    public bool UseTimeAveragedNetworkMeasure
    {
      get { return _useTimeAveragedNetworkMeasure; }
      set
      {
        _useTimeAveragedNetworkMeasure = value;
        OnPropertyChanged();
      }
    }


    private uint _segmentTryLimitOnBitrateSwitch = 2;

    public uint SegmentTryLimitOnBitrateSwitch
    {
      get { return _segmentTryLimitOnBitrateSwitch; }
      set
      {
        _segmentTryLimitOnBitrateSwitch = value;
        OnPropertyChanged();
      }
    }


    private float _MinimumPaddingForBitrateUpshift = 0.40f;//0.25f;

    public float MinimumPaddingForBitrateUpshift
    {
      get { return _MinimumPaddingForBitrateUpshift; }
      set
      {
        _MinimumPaddingForBitrateUpshift = value;
        OnPropertyChanged();
      }
    }

    private float _MaximumToleranceForBitrateDownshift = 0.0f;

    public float MaximumToleranceForBitrateDownshift
    {
      get { return _MaximumToleranceForBitrateDownshift; }
      set
      {
        _MaximumToleranceForBitrateDownshift = value;
        OnPropertyChanged();
      }
    }

    private bool _AllowSegmentSkipOnSegmentFailure = true;

    public bool AllowSegmentSkipOnSegmentFailure
    {
      get { return _AllowSegmentSkipOnSegmentFailure; }
      set
      {
        _AllowSegmentSkipOnSegmentFailure = value;
        OnPropertyChanged();
      }
    }

    private bool _ForceKeyframeMatchOnSeek = true;

    public bool ForceKeyframeMatchOnSeek
    {
      get { return _ForceKeyframeMatchOnSeek; }
      set
      {
        _ForceKeyframeMatchOnSeek = value;
        OnPropertyChanged();
      }
    }

    private bool _UpshiftBitrateInSteps = true;//false;

    public bool UpshiftBitrateInSteps
    {
      get { return _UpshiftBitrateInSteps; }
      set
      {
        _UpshiftBitrateInSteps = value;
        OnPropertyChanged();
      }
    }

    private bool _AutoAdjustScrubbingBitrate = false;

    public bool AutoAdjustScrubbingBitrate
    {
      get { return _AutoAdjustScrubbingBitrate; }
      set
      {
        _AutoAdjustScrubbingBitrate = value;
        OnPropertyChanged();
      }
    }

    private bool _AutoAdjustTrickPlayBitrate = true;

    public bool AutoAdjustTrickPlayBitrate
    {
      get { return _AutoAdjustTrickPlayBitrate; }
      set
      {
        _AutoAdjustTrickPlayBitrate = value;
        OnPropertyChanged();
      }
    }

    private uint _MaximumBitrate = 0;

    public uint MaximumBitrate
    {
      get { return _MaximumBitrate / 1024; }
      set
      {
        _MaximumBitrate = value * 1024;
        OnPropertyChanged();
      }
    }

    private uint _MinimumBitrate = 0;

    public uint MinimumBitrate
    {
      get { return _MinimumBitrate / 1024; }
      set
      {
        _MinimumBitrate = value * 1024;
        OnPropertyChanged();
      }
    }

    private uint _StartBitrate = 0;

    public uint StartBitrate
    {
      get { return _StartBitrate / 1024; }
      set
      {
        _StartBitrate = value * 1024;
        OnPropertyChanged();
      }
    }

    private uint _AudioPID = 0;

    public uint AudioPID
    {
      get { return _AudioPID; }
      set
      {
        _AudioPID = value;
        OnPropertyChanged();
      }
    }

    private uint _VideoPID = 0;

    public uint VideoPID
    {
      get { return _VideoPID; }
      set
      {
        _VideoPID = value;
        OnPropertyChanged();
      }
    }

    private uint _MetadataPID = 0;

    public uint MetadataPID
    {
      get { return _MetadataPID; }
      set
      {
        _MetadataPID = value;
        OnPropertyChanged();
      }
    }

    private SegmentMatchCriterion _matchSegmentUsing;

    public SegmentMatchCriterion MatchSegmentUsing
    {
      get { return _matchSegmentUsing; }
      set { _matchSegmentUsing = value; }
    }


    public event PropertyChangedEventHandler PropertyChanged;
  }

  public class RateDefinition
  {
    private string _Description;

    public string Description
    {
      get { return _Description; }
      set { _Description = value; }
    }

    private double _Value;

    public double Value
    {
      get { return _Value; }
      set { _Value = value; }
    }


  }

  public class CCTypeDefinition
  {
    private string _Description;

    public string Description
    {
      get { return _Description; }
      set { _Description = value; }
    }

    private ClosedCaptionType _Value;

    public ClosedCaptionType Value
    {
      get { return _Value; }
      set { _Value = value; }
    }


  }

  public class BitrateDefinition
  {
    public string Description
    {
      get
      {
        if (_Value == null)
          return "Auto Select";
        else
        {
          return (new BitrateDisplayConverter().Convert(_Value.Value, typeof(String), null, null) as string) + "  " +
            (new BitrateUnitDisplayConverter().Convert(_Value.Value, typeof(String), null, null) as string);
        }

      }
    }

    private uint? _Value;

    public uint? Value
    {
      get { return _Value; }
      set { _Value = value; }
    }
  }

  public class SegmentMatchingCriterionDefinition
  {
    public string Description
    {
      get
      {
        return (_Value == SegmentMatchCriterion.SEQUENCENUMBER ? "Sequence Number" : "Program Date Time");
        //return Enum.GetName(typeof(SegmentMatchCriterion),_Value); 
      }
    }

    private SegmentMatchCriterion _Value;

    public SegmentMatchCriterion Value
    {
      get { return _Value; }
      set { _Value = value; }
    }
  }

}
