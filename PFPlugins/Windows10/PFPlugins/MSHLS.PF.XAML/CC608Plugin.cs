using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
using System.Net.Http;
using System.Threading.Tasks;
using System.Collections.Generic;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Media;
using Windows.Foundation;
using System.Diagnostics;
using Microsoft.CC608;

namespace Microsoft.PlayerFramework.CC608
{
    public class CC608Plugin : PluginBase
    {
        private Panel _captionsContainer;
        private CC608XamlController _controller;

        public CC608Plugin()
        {            
        }

        #region Events
        /// <summary>
        /// Event called right before a UI element is added to the caption 
        /// container.
        /// </summary>
        /// <remarks>Handle this event to modify the Xaml for the captions 
        /// before they are added to the scene.</remarks>
        public event EventHandler<UIElementEventArgs> OnCaptionAdded;
        #endregion

        #region Properties
        /// <summary>
        /// Gets or sets the caption options
        /// </summary>
        public CaptionOptions CaptionOptions
        {
            get
            {
                return new CaptionOptions(_controller.Options);
            }

            set
            {
                _controller.Options = value.GetOptions();
            }
        }
        #endregion

        private Caption _currentTrack;
        public Caption CurrentTrack
        {
            get { return _currentTrack; }
            set
            {
                _controller = new CC608XamlController();
                this.CaptionOptions = new CaptionOptions();

                var oldValue = _currentTrack;

                if (oldValue == value)
                {
                    return;
                }

                if (value != null && this.MediaPlayer.AvailableCaptions.IndexOf(value) == -1)
                {
                    throw new ArgumentException("CurrentTrack can only be set to an object in the AvailableCaptions collection");
                }

                // stop processing the old track
                if (oldValue != null)
                {
                    oldValue.PayloadAugmented -= this.caption_PayloadAugmented;
                }

                // set the controller to start processing the new caption track number
                int trackId = 0;
                if (value != null)
                {
                    int.TryParse(value.Id, out trackId);
                }
                _controller.ActiveCaptionTrack = trackId;

                if (value != null)
                {
                    // wire up to the event to process future data for this track
                    value.PayloadAugmented += this.caption_PayloadAugmented;

                    if (value.Payload != null)
                    {
                        // get the existing data for this new track and immediately process it
                        var queue = value.Payload as Queue<Dictionary<ulong, IList<byte>>>;
                        if (queue != null)
                        {
                            // copy the data to an array for processing (we don't want to dequeue the data, as the user may keep switching back and forth!)
                            var pendingPayloadsArray = new Dictionary<ulong, IList<byte>>[queue.Count];
                            queue.CopyTo(pendingPayloadsArray, 0);

                            foreach (var pendingPayload in pendingPayloadsArray)
                            {
                                ProcessCaptionsData(pendingPayload);
                            }
                        }
                    }
                }

                _currentTrack = value;
                this.MediaPlayer.IsCaptionsActive = (value != null);
            }
        }

        private void MediaPlayer_SelectedCaptionChanged(object sender, RoutedPropertyChangedEventArgs<PlayerFramework.Caption> e)
        {
            // this value could be null, but we still want to set it to turn off the captions
            CurrentTrack = e.NewValue as Caption;

            // temporary test data!
            //controller.InjectTestData();
        }

        private void MediaPlayer_PositionChanged(object sender, RoutedPropertyChangedEventArgs<TimeSpan> e)
        {
            if (MediaPlayer.IsCaptionsActive)
            {
                UpdateCaptionContent();
            }
        }

        private void MediaPlayer_MediaOpened(object sender, RoutedEventArgs e)
        {
            _controller.Reset();
        }

        private void MediaPlayer_SeekCompleted(object sender, RoutedEventArgs e)
        {
            _controller.Seek(MediaPlayer.Position);
        }

        private void captionsContainer_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            if (MediaPlayer.IsCaptionsActive)
            {
                UpdateCaptionContent();
            }
        }

        private void caption_PayloadAugmented(object sender, PayloadAugmentedEventArgs e)
        {
            var payload = e.Payload as IDictionary<ulong, IList<byte>>;   
            ProcessCaptionsData(payload);
        }

        private async void ProcessCaptionsData(IDictionary<ulong, IList<byte>> data)
        {
            if (data != null)
            {
                await _controller.AddNewCaptionDataInUserDataEnvelopeAsync(data);
            }
        }

        /// <inheritdoc /> 
        protected override bool OnActivate()
        {
            var mediaContainer = MediaPlayer.Containers.OfType<Panel>().FirstOrDefault(c => c.Name == MediaPlayerTemplateParts.MediaContainer);
            _captionsContainer = mediaContainer.Children.OfType<Panel>().FirstOrDefault(c => c.Name == MediaPlayerTemplateParts.CaptionsContainer);
            if (_captionsContainer != null)
            {
                _controller = new CC608XamlController();
                this.CaptionOptions = new CaptionOptions();

                MediaPlayer.IsCaptionsActive = (MediaPlayer.SelectedCaption as Caption != null);

                _captionsContainer.SizeChanged += captionsContainer_SizeChanged;
                MediaPlayer.PositionChanged += MediaPlayer_PositionChanged;
                MediaPlayer.SelectedCaptionChanged += MediaPlayer_SelectedCaptionChanged;
                MediaPlayer.MediaOpened += MediaPlayer_MediaOpened;
                MediaPlayer.SeekCompleted += MediaPlayer_SeekCompleted;

                return true;
            }
            return false;
        }

        /// <inheritdoc /> 
        protected override void OnDeactivate()
        {
            MediaPlayer.PositionChanged -= MediaPlayer_PositionChanged;
            MediaPlayer.SelectedCaptionChanged -= MediaPlayer_SelectedCaptionChanged;
            _captionsContainer.SizeChanged -= captionsContainer_SizeChanged;
            MediaPlayer.MediaOpened -= MediaPlayer_MediaOpened;
            MediaPlayer.SeekCompleted -= MediaPlayer_SeekCompleted;

            MediaPlayer.IsCaptionsActive = false;
            _captionsContainer.Children.Clear();
            _captionsContainer = null;
        }

        /// <summary>
        /// Update the caption content
        /// </summary>
        private async void UpdateCaptionContent()
        {
            var updateRequired = await _controller.CaptionUpdateRequiredAsync(MediaPlayer.Position);

            if (updateRequired)
            {
                var captionData = _controller.GetXamlCaptions(
                    (ushort)MediaPlayer.ActualHeight);

                if (captionData.RequiresUpdate)
                {
                    _captionsContainer.Children.Clear();

                    // If an OnCaptionAdded event handler has been added, call 
                    // it to modify the caption Xaml before adding it to the 
                    // scene.
                    if (this.OnCaptionAdded != null)
                    {
                        this.OnCaptionAdded(this, new UIElementEventArgs(captionData.CaptionsRoot));
                    }

                    _captionsContainer.Children.Add(captionData.CaptionsRoot);
                }
            }
        }
    }
}
