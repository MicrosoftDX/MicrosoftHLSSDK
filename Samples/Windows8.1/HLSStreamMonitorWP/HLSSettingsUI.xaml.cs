using System;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using Windows.UI.Xaml.Controls;

// The User Control item template is documented at http://go.microsoft.com/fwlink/?LinkId=234236

namespace HLSStreamMonitorWP
{
  public sealed partial class HLSSettingsUI : UserControl
  {
 
    public HLSSettingsUI()
    {
      this.InitializeComponent();
    }
  }


  public class HLSSettings : INotifyPropertyChanged
  {

    public event PropertyChangedEventHandler PropertyChanged;
    private void OnPropertyChanged([CallerMemberName] string PropName = "")
    {
      if (PropertyChanged != null)
        PropertyChanged(this, new PropertyChangedEventArgs(PropName));
    }

    event EventHandler<EventArgs> SettingsApplied;

    private TimeSpan _minimumBufferLength = TimeSpan.FromSeconds(50);

    public TimeSpan MinimumBufferLength
    {
      get { return _minimumBufferLength; }
      set
      {
        _minimumBufferLength = value;
        OnPropertyChanged();
      }
    }

    
    

    private TimeSpan _bitrateChangeNotificationInterval = TimeSpan.FromSeconds(5);

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

    private bool _isWebVTTCaption = false;

    public bool IsWebVTTCaption
    {
      get { return _isWebVTTCaption; }
      set
      {
        _isWebVTTCaption = value;
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
 

    internal uint _MaximumBitrate = 0;

    public uint MaximumBitrate
    {
      get { return _MaximumBitrate; }
      set
      {
        _MaximumBitrate = value;
        OnPropertyChanged();
      }
    }

    private uint _MinimumBitrate = 0;

    public uint MinimumBitrate
    {
      get { return _MinimumBitrate; }
      set
      {
        _MinimumBitrate = value;
        OnPropertyChanged();
      }
    }

    private uint _StartBitrate = 0;

    public uint StartBitrate
    {
      get { return _StartBitrate; }
      set
      {
        _StartBitrate = value;
        OnPropertyChanged();
      }
    }

    private ushort _AudioPID = 0;

    public ushort AudioPID
    {
      get { return _AudioPID; }
      set
      {
        _AudioPID = value;
        OnPropertyChanged();
      }
    }

    private ushort _VideoPID = 0;

    public ushort VideoPID
    {
      get { return _VideoPID; }
      set
      {
        _VideoPID = value;
        OnPropertyChanged();
      }
    }



    private bool _UpshiftBitrateInSteps = true;

    public bool UpshiftBitrateInSteps
    {
      get
      {
        return _UpshiftBitrateInSteps;
      }

      set
      {
        if (value != _UpshiftBitrateInSteps)
        {
          _UpshiftBitrateInSteps = value;
          OnPropertyChanged();
        }

      }
    }

    
    
   
  }
}
