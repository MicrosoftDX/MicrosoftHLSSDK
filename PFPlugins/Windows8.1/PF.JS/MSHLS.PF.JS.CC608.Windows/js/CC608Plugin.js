(function (PlayerFramework, undefined) {

    "use strict";

    var CC608Plugin = WinJS.Class.derive(PlayerFramework.PluginBase, function (options) {
        if (!(this instanceof PlayerFramework.Plugins.CC608Plugin)) {
            throw invalidConstruction;
        }

        this._captionsContainerElement = null;
        this._controller = new Microsoft.CC608.CC608HtmlController();

        PlayerFramework.PluginBase.call(this, options);
    }, {

        // public properties

        currentTrack: {
            get: function () {
                return this.mediaPlayer._currentCaptionTrack;
            },
            set: function (value) {

                var oldValue = this.mediaPlayer._currentCaptionTrack;
                if (oldValue !== value) {
                    // validate new track
                    if (value && this.mediaPlayer.captionTracks.indexOf(value) === -1) {
                        throw invalidCaptionTrack;
                    }

                    // stop processing old track
                    if (oldValue) {
                        if (oldValue instanceof PlayerFramework.DynamicTextTrack) {
                            this._unbindEvent("payloadaugmented", oldValue, this._onPayloadAugmented);
                        }
                    }

                    var trackId = (value == null) ? 0 : value._stream.id;
                    this._controller.activeCaptionTrack = trackId;

                    // start processing new track and send any pending data for processing, now that the controller knows which track we want
                    if (value) {
                        if (value instanceof PlayerFramework.DynamicTextTrack) {
                            this._bindEvent("payloadaugmented", value, this._onPayloadAugmented);

                            // make sure that the data format is as expected (otherwise ignore it)
                            if (value._stream &&
                                value._stream.ccPayload &&
                                Array.isArray(value._stream.ccPayload)) {

                                // iterate over all the recent data and re-process it for this caption track
                                for (var i = 0; i < value._stream.ccPayload.length; ++i) {
                                    this._processPayload(value._stream.ccPayload[i]);
                                }
                            }
                        }
                        this.show();
                    } else {
                        this.hide();
                    }


                    this.mediaPlayer._currentCaptionTrack = value;
                    this.mediaPlayer._observableMediaPlayer.notify("currentCaptionTrack", value, oldValue);
                    this.mediaPlayer.dispatchEvent("currentcaptiontrackchange");

                    // temp test code
                    //if (value) {
                    //    this._controller.injectTestData();
                    //}
                }
            }
        },

        // plugin overrides
        _onLoad: function () {
        },

        _onUnload: function () {
        },

        _onUpdate: function () {
        },

        // public methods
        show: function () {
            this.mediaPlayer.addClass("pf-show-608-captions-container");
        },

        hide: function () {
            this.mediaPlayer.removeClass("pf-show-608-captions-container");
        },

        // Private Methods

        _onPayloadAugmented: function (e) {
            if (!e.detail || !e.detail.payload) {
                return;
            }

          try{
            // check if the HLS payload type exists
            if (e.detail.payload.HLSInbandCCPayloadArray) {
              var payload = e.detail.payload;
              this._processPayload(payload);
            }
          }
          catch(ex)
          {

          }

            // TODO: if other captions formats are necessary, extend them here
        },

        _processPayload: function (payload) {

          try{
            if (!payload || !payload.HLSInbandCCPayloadArray) {
              // no data to process
              return;
            }

            var data = payload.HLSInbandCCPayloadArray;

            var raw = new Microsoft.CC608.RawCaptionData();
            var length = data.length;

            for (var i = 0; i != length; ++i) {
              var element = data[i];
              var bytes = element.getPayload();
              if (bytes) {
                raw.addByArray(element.timestamp * 10000, bytes);
              }
            }

            this._controller.addNewCaptionDataInUserDataEnvelopeAsync(raw);
          }
          catch(ex)
          {}
        },

        _setElement: function () {
            this._captionsContainerElement = PlayerFramework.Utilities.createElement(this.mediaPlayer.element, ["div", { "class": "pf-608-captions-container" }]);
        },

        _setOptions: function (options) {
            PlayerFramework.Utilities.setOptions(this, options, {
                isEnabled: true
            });
        },

        _onActivate: function () {
            this._setElement();

            this._bindEvent("loading", this.mediaPlayer, this._onMediaPlayerLoading);
            this._bindEvent("timeupdate", this.mediaPlayer, this._onMediaPlayerTimeUpdate);
            this._bindEvent("currentcaptiontrackchanging", this.mediaPlayer, this._onMediaPlayerCurrentCaptionTrackChanging);
            this._bindEvent("resize", this.mediaPlayer.element, this._onMediaPlayerResize);
            this._bindEvent("seeked", this.mediaPlayer, this._onMediaPlayerSeeked);

            this._updateCaptionContainerSize();
        },

        _onDeactivate: function () {
            this._unbindEvent("loading", this.mediaPlayer, this._onMediaPlayerLoading);
            this._unbindEvent("timeupdate", this.mediaPlayer, this._onMediaPlayerTimeUpdate);
            this._unbindEvent("currentcaptiontrackchanging", this.mediaPlayer, this._onMediaPlayerCurrentCaptionTrackChanging);
            this._unbindEvent("resize", this.mediaPlayer.element, this._onMediaPlayerResize);
            this._unbindEvent("seeked", this.mediaPlayer, this._onMediaPlayerSeeked);

            PlayerFramework.Utilities.removeElement(this._captionsContainerElement);
            this._captionsContainerElement = null;
        },

        _onMediaPlayerLoading: function (e) {
            this._controller.reset();
        },

        _onMediaPlayerTimeUpdate: function (e) {
            if (this.currentTrack)
                this._updateCaptionContent();
        },

        _onMediaPlayerCurrentCaptionTrackChanging: function (e) {
            this.currentTrack = e.detail.track;
            e.preventDefault();
        },

        _onMediaPlayerResize: function (e) {
            this._updateCaptionContainerSize();
        },

        _onMediaPlayerSeeked: function (e) {
            this._controller.seek(this.mediaPlayer.currentTime);
        },

        _updateCaptionContainerSize: function (e) {
            if (this.currentTrack)
                this._updateCaptionContent();
        },

        _updateCaptionContent: function () {
            /// <summary>Update the caption contents</summary>
            var that = this;
            var currentSize = PlayerFramework.Utilities.measureElement(this.mediaPlayer.element);

            // account for a different resolution scale than 100%
            var displayInformation = Windows.Graphics.Display.DisplayInformation.getForCurrentView();

            var scale = displayInformation.resolutionScale;

            var aspect = currentSize.width / currentSize.height;

            var height = currentSize.height;

            if (aspect >= 1) {
                height = height * 100.0 / scale;
            }
            else {
                // Vertical format players (aspect ratio < 1) need to make the 
                // captions even smaller so they fit within the player
                height = aspect * height * 100.0 / scale;
            }

            // convert to ms from seconds before projecting into WinRT
            var currentTimeMilliSec = this.mediaPlayer.currentTime * 1000;

            this._controller.getHtmlCaptionsAsync(currentTimeMilliSec, height).then(
                function (e) {
                    if (e.requiresUpdate) {
                        that._captionsContainerElement.innerHTML = e.captionMarkup;

                        // If there is a Caption Settings Plug-in, ask it to 
                        // modify the caption DOM once its update
                        var captionSettings = PlayerFramework.CaptionSettings;

                        if (captionSettings) {
                            captionSettings.captionsUpdated(that._captionsContainerElement);
                        }
                    }
                });
        }
    });

    // CC608Plugin Mixins
    WinJS.Class.mix(PlayerFramework.MediaPlayer, {
        CC608Plugin: {
            value: null,
            writable: true,
            configurable: true
        }
    });

    // CC608Plugin Exports
    WinJS.Namespace.define("PlayerFramework.Plugins", {
        CC608Plugin: CC608Plugin
    });

})(PlayerFramework);
