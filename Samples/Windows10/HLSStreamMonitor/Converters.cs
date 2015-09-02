using System;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Data;

// The User Control item template is documented at http://go.microsoft.com/fwlink/?LinkId=234236

namespace HLSStreamMonitor
{

  public class TimeSpanToMilliSecondsDisplayConverter : IValueConverter
  {

    public object Convert(object value, Type targetType, object parameter, string language)
    {
      if (value != null)
      {
        TimeSpan ts = (TimeSpan)value;
        if (ts == default(TimeSpan))
          return "";
        else
          return ts.TotalMilliseconds.ToString();
      }
      else
        return "";
    }

    public object ConvertBack(object value, Type targetType, object parameter, string language)
    {
      string val = (String)value;
      if (string.IsNullOrEmpty(val) || val.Trim() == string.Empty)
        return default(TimeSpan);
      else
      {
        try{
          return TimeSpan.FromMilliseconds(System.Convert.ToInt64(val));
        }
        catch
        {
          return default(TimeSpan);
        }
      }
    }
  }
  public class TimeDisplayConverter : IValueConverter
  {

    public object Convert(object value, Type targetType, object parameter, string language)
    {
      if (value is TimeSpan)
      {
        TimeSpan ts = (TimeSpan)value;
        if (ts.TotalDays > 1)
          return ts.ToString(@"dd' days 'hh\:mm\:ss\.fff");
        else if (ts.TotalHours > 1)
          return ts.ToString(@"hh\:mm\:ss\.fff");
        else
          return ts.ToString(@"mm\:ss\.fff");
      }
      else if (value is DateTime)
      {
        return ((DateTime)value).ToString(@"hh\:mm\:ss\.fff");
      }
      else
        return null;
    }

    public object ConvertBack(object value, Type targetType, object parameter, string language)
    {
      throw new NotImplementedException();
    }
  }

  public class BitrateDisplayConverter : IValueConverter
  {

    public object Convert(object value, Type targetType, object parameter, string language)
    {
      ulong Bitrate = 0;
      if(value == null)
        return "";
      if(value is uint?)
      {
        Bitrate = ((uint?)value).Value;
      }
      else
       Bitrate = (ulong)value;
      if ((Bitrate / (1024 * 1024)) < 1)
        return string.Format("{0:###}", (double)Bitrate / 1024);
      else
        return string.Format("{0:##.#}", (double)Bitrate / (1024 * 1024));
    }

    public object ConvertBack(object value, Type targetType, object parameter, string language)
    {
      throw new NotImplementedException();
    }
  }

  public class BitrateUnitDisplayConverter : IValueConverter
  {

    public object Convert(object value, Type targetType, object parameter, string language)
    {
      ulong Bitrate = 0;
      if (value == null)
        return "";
      if (value is uint?)
      {
        Bitrate = ((uint?)value).Value;
      }
      else
        Bitrate = (ulong)value;
      if ((Bitrate / (1024 * 1024)) < 1)
        return "kbps";
      else
        return "mbps";
    }

    public object ConvertBack(object value, Type targetType, object parameter, string language)
    {
      throw new NotImplementedException();
    }
  }

  public class BoolToVisibilityConverter : IValueConverter
  {

    public object Convert(object value, Type targetType, object parameter, string language)
    {
      if (parameter != null && (string)parameter == "reverse")
      {
        return (bool)value == false ? Visibility.Visible : Visibility.Collapsed;
      }
      else
      {
        return (bool)value == true ? Visibility.Visible : Visibility.Collapsed;
      }
    }

    public object ConvertBack(object value, Type targetType, object parameter, string language)
    {
      throw new NotImplementedException();
    }
  }
}