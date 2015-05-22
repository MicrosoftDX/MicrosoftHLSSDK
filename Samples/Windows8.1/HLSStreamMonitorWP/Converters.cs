using System;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Data;

// The User Control item template is documented at http://go.microsoft.com/fwlink/?LinkId=234236

namespace HLSStreamMonitorWP
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
        try
        {
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
      else if((value is ulong || value is long || value is double))
      {
        
        if(System.Convert.ToInt64(value) == 0.0)
        {
          return "00:00.00";
        }


        TimeSpan ts = TimeSpan.FromTicks(System.Convert.ToInt64(value));
        if (ts.TotalDays > 1)
          return ts.ToString(@"dd' days 'hh\:mm\:ss\.fff");
        else if (ts.TotalHours > 1)
          return ts.ToString(@"hh\:mm\:ss\.fff");
        else
          return ts.ToString(@"mm\:ss\.fff");
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
      var Bitrate = (uint)value;
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
      var Bitrate = (uint)value;
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

  public class VisibilityToBoolConverter : IValueConverter
  {

    public object Convert(object value, Type targetType, object parameter, string language)
    {

      return (Visibility)value == Visibility.Visible ? true : false;
    }
       

    public object ConvertBack(object value, Type targetType, object parameter, string language)
    {
      throw new NotImplementedException();
    }
  }
}