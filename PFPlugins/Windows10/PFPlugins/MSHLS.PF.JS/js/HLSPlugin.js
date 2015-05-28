(function (PlayerFramework, undefined) {

    "use strict";

    // enum for closed caption options
    var ClosedCaptionType = {
        none: 0,
        CC608Instream: 1
    };

    var HLSPlugin = WinJS.Class.derive(PlayerFramework.PluginBase, function (options) {
        if (!(this instanceof PlayerFramework.Plugins.HLSPlugin)) {
            throw invalidConstruction;
        }
        // shared WinRT component (internal)
        this._manager = new Microsoft.HLSClientExtensions.HLSManager();

        // SDK controller
        this._controller = null;

        // closed caption option
        this._closedCaptionType = ClosedCaptionType.none;

        PlayerFramework.PluginBase.call(this, options);
    }, {

        // public properties

        startupBitrate: {
            get: function () {
                return this._manager.startupBitrate;
            },
            set: function (value) {
                var oldValue = this._manager.startupBitrate;
                if (oldValue !== value) {
                    this._manager.startupBitrate = value;
                    this._observablePlugin.notify("startupBitrate", value, oldValue);
                }
            }
        },

        minBitrate: {
            get: function () {
                return this._manager.minBitrate;
            },
            set: function (value) {
                var oldValue = this._manager.minBitrate;
                if (oldValue !== value) {
                    this._manager.minBitrate = value;
                    this._observablePlugin.notify("minBitrate", value, oldValue);
                }
            }
        },

        maxBitrate: {
            get: function () {
                return this._manager.maxBitrate;
            },
            set: function (value) {
                var oldValue = this._manager.maxBitrate;
                if (oldValue !== value) {
                    this._manager.maxBitrate = value;
                    this._observablePlugin.notify("maxBitrate", value, oldValue);
                }
            }
        },

        closedCaptionType: {
            get: function () {
                return this._closedCaptionType;
            },
            set: function (value) {
                if (this._closedCaptionType !== value) {
                    this._closedCaptionType = value;
                    this._updateCaptionTracks();
                }
            }
        },

        controllerFactory: {
            get: function () {
                return this._manager.controllerFactory;
            }
        },

        // Adds non-standard Media Foundation registration for the HLS Client SDK.
        addNonStandardRegistration: function (fileExtension, mimeType) {
            if (!fileExtension)
                throw ("fileExtension parameter required--cannot be null or empty string");

            if (!mimeType)
                throw ("mimeType parameter to addNonStandardRegistration required--cannot be null or empty string!");

            this._manager.addNonStandardRegistration(fileExtension, mimeType);
        },

        // public events:
        // * hlscontrollerready -- provides access (via the event args) to the controller object from the HLS Client SDK when a video stream starts up for advanced scenarios
        // * bitrateswitchsuggestedcannotcancel -- informs app that bitrate may soon change
        // * bitrateswitchcompleted -- informs app that the bitrate just changed
        // * bitrateswitchcancelled -- informs app that a pending bitrate switch was cancelled
        // * streamselectionchanged -- informs app that the streams available changed--commonly fires when switching to or from an audio-only stream and a stream with both video and audio
        // * availablecaptionspopulated -- informs app that caption tracks have been created, and allows the app to customize or delete certain track objects
        // * instream608captionsavailable -- informs app that there really is 608 data in the stream being played (fires each time 608 data is found in-stream)

        // plugin overrides
        _onLoad: function () {
        },

        _onUnload: function () {
            if (!this._manager) {
                this._manager.uninitialize();
                this._manager = null;
            }
        },

        _onUpdate: function () {
        },

        _onActivate: function () {

            // Currently, the HLS SDK is not capable of handling the rapid-fire seek requests generated if 
            // seekWhileScrubbing is true, so turning it off by default.  If the HLS SDK is updated to tolerate
            // rapid-fire seek requests, this should be removed.
            this.mediaPlayer.seekWhileScrubbing = false;

            this._bindEvent("loading", this.mediaPlayer, this._onMediaPlayerLoading);
            this._bindEvent("canplay", this.mediaPlayer, this._onCanPlay);
            this._bindEvent("currentaudiotrackchanging", this.mediaPlayer, this._onMediaPlayerCurrentAudioTrackChanging);

            this._bindEvent("bitrateswitchsuggestedcannotcancel", this._manager, this._onBitrateSwitchSuggestedCannotCancel);
            this._bindEvent("bitrateswitchcompleted", this._manager, this._onBitrateSwitchCompleted);
            this._bindEvent("bitrateswitchcancelled", this._manager, this._onBitrateSwitchCancelled);
            this._bindEvent("streamselectionchanged", this._manager, this._onStreamSelectionChanged);
            this._bindEvent("hlscontrollerready", this._manager, this._onHLSControllerReady);

            this._bindEvent("segmentswitched", this._manager, this._onSegmentSwitched);

            return true;
        },

        _onDeactivate: function () {
            this._unbindEvent("loading", this.mediaPlayer, this._onMediaPlayerLoading);
            this._unbindEvent("canplay", this.mediaPlayer, this._onCanPlay);
            this._unbindEvent("currentaudiotrackchanging", this.mediaPlayer, this._onMediaPlayerCurrentAudioTrackChanging);

            this._unbindEvent("bitrateswitchsuggestedcannotcancel", this._manager, this._onBitrateSwitchSuggestedCannotCancel);
            this._unbindEvent("bitrateswitchcompleted", this._manager, this._onBitrateSwitchCompleted);
            this._unbindEvent("bitrateswitchcancelled", this._manager, this._onBitrateSwitchCancelled);
            this._unbindEvent("streamselectionchanged", this._manager, this._onStreamSelectionChanged);
            this._unbindEvent("hlscontrollerready", this._manager, this._onHLSControllerReady);

            this._unbindEvent("segmentswitched", this._manager, this._onSegmentSwitched);
        },

        _onMediaPlayerLoading: function (e) {
            if (!this._manager.isInitialized) { // only do this the first time
                this._manager.initialize(this.mediaPlayer.mediaExtensionManager);
            }

            this._manager.sourceUri = new Windows.Foundation.Uri(e.detail.src);
        },

        _onHLSControllerReady: function (e) {
            // save SDK controller
            this._controller = e;

            // If mediaPlayer.audioTracks is null at this point, mediaPlayer will use the media element's audio tracks
            // which is not what we want. But the HLS SDK audio tracks are not (necessarily) ready by this event, so we 
            // set mediaPlayer.audioTracks to something, and the audio tracks are set later from within the can play event.
            this.mediaPlayer.audioTracks = [];

            // public event for apps (just send the controller as the event args)
            this.dispatchEvent("hlscontrollerready", e.detail[0]);
        },

        _onCanPlay: function (e) {
            // perform any initialization that is required after 
            // the player has enough data to start video playback

            // (currently alternate audio renditions are not supported)
            // TODO: Uncomment call to _parseAudioRenditions when alternate audio is supported
            //this._parseAudioRenditions();

            this._refreshState();
            this._updateCaptionTracks();
        },

        _onBitrateSwitchSuggestedCannotCancel: function (e) {
            // public event for apps
            this.dispatchEvent("bitrateswitchsuggestedcannotcancel", e.detail[0]);
        },

        _onBitrateSwitchCompleted: function (e) {
            // reparse the audio renditions (in case they changed)
            // TODO: Uncomment call to _parseAudioRenditions when alternate audio renditions are supported in the HLS SDK
            //this._parseAudioRenditions();

            this._refreshState();

            // public event for apps
            this.dispatchEvent("bitrateswitchcompleted", e.detail[0]);
        },

        _onBitrateSwitchCancelled: function (e) {
            // public event for apps
            this.dispatchEvent("bitrateswitchcancelled", e.detail[0]);
        },

        _onStreamSelectionChanged: function (e) {
            // public event for apps
            this.dispatchEvent("streamselectionchanged", e.detail[0]);
        },

        _onMediaPlayerCurrentAudioTrackChanging: function (e) {
            // only process this event if the track object has the _hiddenId--otherwise, the app developer has added their own 
            // audio tracks to the audio track selection control, and then we don't know how to map back to the SDK renditions.
            // In this case, the app developer needs to handle this event and set the active track
            if (!e.detail.track || !e.detail.track._hiddenId) {
                return;
            }

          try{
            var audioRenditions = this._getAudioRenditions();
            
            // look for the audio rendition that has the same hiddenId as the selected audio track from the event args--when found, active it!
            for (var i = 0; i !== audioRenditions.length; ++i) {
              var hiddenId = audioRenditions[i].name + '+' + audioRenditions[i].language;
              if (hiddenId === e.detail.track._hiddenId) {
                audioRenditions[i].isActive = true;
                break;
              }
            }
          }
          catch(ex)
          {}
        },

        _onSegmentSwitched: function (e) {

          try{
            // refresh state here (every segment change) rather than bitrate switches, as the SDK object model itself will be changing around the bitrate switch,
            // leading to possible null references and such
            this._refreshState();

            if (this._closedCaptionType !== ClosedCaptionType.CC608Instream)
              return;

            var toSegment = e.detail[0].toSegment;

            if (toSegment.mediaType === 1) { // AUDIO
              // no need to process audio track, as it will never contain 608 caption data
              return;
            }

            var payloads = toSegment.getInbandCCUnits();

            if (payloads === null) {
              // no data--nothing to process
              return;
            }

            // let the app know that we do indeed have 608 data in this stream
            this.dispatchEvent("instream608captionsavailable", {});

            // create a new (empty) object, and put the raw data on a property of it, so that it can be easily identified
            var payloadCarrier = {};
            payloadCarrier.HLSInbandCCPayloadArray = payloads;

            if (this.mediaPlayer.currentCaptionTrack) {
              // we have a current track--signal it to process the data
              this.mediaPlayer.currentCaptionTrack.augmentPayload(payloadCarrier);
            }

            // now add the data to ALL the caption tracks, and the latest version will be processed if that caption track is selected

            // first make sure we have caption tracks
            if (!this.mediaPlayer.captionTracks) {
              // no tracks--nothing to save to
              return;
            }

            // stick the current payload to all the caption objects for later processing when they are selected
            var len = this.mediaPlayer.captionTracks.length;
            for (var i = 0; i !== len; ++i) {

              // get a simpler reference to the necessary element
              var ct = this.mediaPlayer.captionTracks[i];

              // make sure we have an array to store the most recent payloads in
              if (!ct._stream ||
                  !ct._stream.ccPayload ||
                  !Array.isArray(ct._stream.ccPayload)) {
                ct._stream.ccPayload = [];
              }

              // put the newest payload on the end
              ct._stream.ccPayload.push(payloadCarrier);

              // and get rid of the older ones on the front (only keep the last three, as we could be as much as 30 seconds ahead)
              while (ct._stream.ccPayload.length > 3) {
                ct._stream.ccPayload.shift();
              }
            }
          }
          catch(ex)
          {}
        },

        _refreshState: function () {
            try {

                // get the active variant stream into a local variable to avoid null references as it's null briefly while switching bitrates
                var activeStream = this._controller.playlist.activeVariantStream;

                if (activeStream === null) {
                    return;
                }

                // value ranging from 0 to 1
                this.mediaPlayer.signalStrength = (this._controller.playlist.maximumAllowedBitrate === 0) ? 0 :
                    activeStream.bitrate / this._controller.playlist.maximumAllowedBitrate;

                // HLS has an optional resolution, only set media quality if the HLS stream specifies a resolution
                if (activeStream.hasResolution) {
                    this.mediaPlayer.mediaQuality = (activeStream.verticalResolution >= 720) ?
                        PlayerFramework.MediaQuality.highDefinition : PlayerFramework.MediaQuality.standardDefinition;
                }
            } catch (ex) {
                // swallow exceptions as they could be due to 404 errors leading to the SDK object model collapsing
            }
        },

        // TODO: Make sure that if _getAudioRenditions() doesn't return valid data (because the ActiveVariantStream was null) that we have some way 
        // of correcting this and getting the correct data soon thereafter. For example, consider calling this on every segment switch. 
        // (Otherwise, if we just call it when the bitrate shifts, if we don't get valid data that time, it may never shift again...)
        _parseAudioRenditions: function () {

          try{
            var audioRenditions = this._getAudioRenditions();

            // TODO: verify that this is the correct logic here (if nothing is active, then assume the default track is selected)
            var audioTracks = [];
            var defaultTrack = null;
            var foundActiveTrack = null;
            for (var i = 0; i < audioRenditions.length; ++i) {
              var audioStream = audioRenditions[i];
              var audioTrack = { label: audioStream.name, language: audioStream.language, enabled: audioStream.isActive, _hiddenId: audioStream.name + '+' + audioStream.language };
              audioTracks.push(audioTrack);

              // if we found an active track, no need to pick the default one
              if (audioTrack.enabled) {
                foundActiveTrack = audioTrack;
              }

              // save for later if necessary
              if (audioStream.isDefault) {
                defaultTrack = audioStream;
              }
            }

            // use the default track as the active one if we didn't find an active track
            if (!foundActiveTrack) {
              for (var i = 0; i < audioRenditions.length; ++i) {
                if (audioTracks[i].label === defaultTrack.name) {
                  audioTracks[i].enabled = true;
                  foundActiveTrack = audioTracks[i];
                }
              }
            }

            var same = false;

            // due to race conditions with changing the existing collection in mid-playback, only
            // change the actual collection if it is different
            if (this.mediaPlayer.audioTracks &&
                this.mediaPlayer.audioTracks.length === audioTracks.length) {

              same = true;

              // check each value to see if it's the same
              for (var i = 0; i < audioTracks.length; ++i) {
                if (!this.mediaPlayer.audioTracks[i]._hiddenId) {
                  // in this case, the app developer added their own data, and we don't want to mess with it
                  same = true;
                  break;
                }

                if (this.mediaPlayer.audioTracks[i]._hiddenId !== audioTracks[i]._hiddenId) {
                  same = false;
                  break;
                }
              }
            }

            // now only change if the collections are different
            if (!same) {
              this.mediaPlayer.audioTracks = audioTracks;

              if (foundActiveTrack) {
                this.mediaPlayer.currentAudioTrack = foundActiveTrack;
              }
            }
          }
          catch(ex){}
        },

        _getAudioRenditions: function () {
            try {
                if (this._controller === null ||
                    this._controller.playlist === null ||
                    !this._controller.playlist.isMaster) {
                    return [];
                }

                // get this value into a local variable as it can change and briefly be null during bitrate switching
                var activeStream = this._controller.playlist.activeVariantStream;

                if (activeStream === null) {
                    return [];
                }

                var audioRenditions = activeStream.getAudioRenditions();

                if (audioRenditions === null) {
                    return [];
                }

                return audioRenditions;
            } catch (ex) {
                // swallow exceptions as they could be due to 404s causing the SDK object model to collapse
            }
        },

        _updateCaptionTracks: function () {
            if (this.mediaPlayer === null)
                return;

            // note: do not set the captionTracks here in case sidecar files are in play

            this.mediaPlayer.currentCaptionTrack = null;
            if (this._closedCaptionType === ClosedCaptionType.CC608Instream) {
                var captionTracks = [];
                captionTracks.push(new PlayerFramework.DynamicTextTrack({ id: "1", name: "CC1" }));
                captionTracks.push(new PlayerFramework.DynamicTextTrack({ id: "2", name: "CC2" }));
                captionTracks.push(new PlayerFramework.DynamicTextTrack({ id: "3", name: "CC3" }));
                captionTracks.push(new PlayerFramework.DynamicTextTrack({ id: "4", name: "CC4" }));
                this.mediaPlayer.captionTracks = captionTracks;

                // raise event so app can override the available captions
                this.dispatchEvent("availablecaptionspopulated", {});
            }
        }
    });

    // HLSPlugin Mixins
    WinJS.Class.mix(PlayerFramework.MediaPlayer, {
        hlsPlugin: {
            value: null,
            writable: true,
            configurable: true
        }
    });

    // HLSPlugin Exports
    WinJS.Namespace.define("PlayerFramework.Plugins", {
        HLSPlugin: HLSPlugin
    });

})(PlayerFramework);
