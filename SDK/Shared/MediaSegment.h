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

#pragma once

#define DEFAULT_AUDIOFRAME_LENGTH 100000//333334

#include <map> 
#include <vector>
#include <mutex>
#include <algorithm>
#include <memory> 
#include <ppltasks.h>
#include <mfidl.h>
#include "SampleData.h"
#include "TSConstants.h" 
#include "TransportStreamParser.h" 
#include "ContentDownloader.h" 


using namespace std;
using namespace Concurrency;

namespace Microsoft {
  namespace HLSClient {
    namespace Private {



      class Playlist;
      class EncryptionKey;
      class SampleData;
      class ContentDownloadRegistry;
      class CHLSMediaSource;
      ///<summary>Various states that a segment can be in</summary>
      enum MediaSegmentState
      {
        ///<summary>Stored in memory in internal segment buffer</summary>
        INMEMORYCACHE,
        ///<summary>Stored on disk (Not Yet Implemented)</summary>
        INSTORAGECACHE,
        ///<summary>Downloading</summary>
        DOWNLOADING,
        ///<summary>We either know the length because it is a byte range, or it was downloaded, played and then scavenged - but we still know the length</summary>
        LENGTHONLY,
        ///<summary>No action taken yet</summary>
        UNAVAILABLE
      };

      enum UnresolvedTagPlacement
      {
        PreSegment, WithSegment, PostSegment
      };

      enum TimestampMatch
      {
        Exact,Closest, ClosestGreaterOrEqual,ClosestLesserOrEqual,ClosestGreater,ClosestLesser,
      };



      class SegmentTSData
      {
      public:

        unique_ptr<BYTE[]> buffer;

        std::vector<shared_ptr<SampleData>> CCSamples;
        ///<summary>Sample queue for samples yet to be read</summary>
        std::map<unsigned short, std::deque<std::shared_ptr<SampleData>>> UnreadQueues; 
        ///<summary>Maps content type to PID</summary>
        std::map<ContentType, unsigned short> MediaTypePIDMap;
        std::vector<unsigned short> MetadataStreams; 
        ///<summary>All the timestamps in the segment</summary>
        std::vector<std::shared_ptr<Timestamp>> Timeline;

        SegmentTSData() {}

        SegmentTSData(SegmentTSData& copyfrom) = delete;

        SegmentTSData(SegmentTSData&& moveFrom)
        {
          CCSamples = std::move(moveFrom.CCSamples);
          UnreadQueues = std::move(moveFrom.UnreadQueues); 
          MediaTypePIDMap = std::move(moveFrom.MediaTypePIDMap);
          MetadataStreams = std::move(moveFrom.MetadataStreams);
          Timeline = std::move(moveFrom.Timeline); 
        }
      };
      ///<summary>Type represents a media segment</summary>
      ///<remarks>The type encapsulates all the necessary data and metadata, including elementary stream data, TS segment data and samples as well as methods to manipulate them</remarks>
      class MediaSegment  // : public CDownloadTarget
      {
        friend class Playlist;
      private:
        //current state the segment instance is in
        MediaSegmentState State; 

        std::map<wstring, shared_ptr<SegmentTSData>> backbuffer;
        /*shared_ptr<SegmentTSData> buffer;*/
        unique_ptr<BYTE[]> buffer;

        volatile short chainAssociationCount;
        
        HRESULT BuildAudioElementaryStreamSamples();
        HRESULT BuildAudioElementaryStreamSamples(shared_ptr<SegmentTSData> buffer);

        static ULONG HasAACTimestampTag(const BYTE *tsdata, ULONG size);
        static shared_ptr<Timestamp> ExtractInitialTimestampFromID3PRIV(const BYTE *tsdata, ULONG size); 
       
       
        shared_ptr<std::map<ContentType, unsigned short>> spPIDFilter;
        void ApplyPIDFilter(std::map<ContentType, unsigned short> pidfilter, bool Force = false);

      public:
        //instance lock
        //  CriticalSection csRoot;
        std::recursive_mutex LockSegment;
        shared_ptr<ContentDownloadRegistry> spDownloadRegistry;
        //pointer to the parent playlist
        Playlist *pParentPlaylist;
         
        ///<summary>Normalized start timestamp</summary>
        shared_ptr<Timestamp> StartPTSNormalized;
        ///<summary>Normalized end timestamp</summary>
        shared_ptr<Timestamp> EndPTSNormalized;
        
        
        std::vector<shared_ptr<SampleData>> CCSamples;
        ///<summary>Sample queue for samples yet to be read</summary>
        std::map<unsigned short, std::deque<std::shared_ptr<SampleData>>> UnreadQueues;
        ///<summary>Sample queue for samples that have been read</summary>
        std::map<unsigned short, std::deque<std::shared_ptr<SampleData>>> ReadQueues;
        ///<summary>Maps content type to PID</summary>
        std::map<ContentType, unsigned short> MediaTypePIDMap;
        std::vector<unsigned short> MetadataStreams; 
        ///<summary>All the timestamps in the segment</summary>
        std::vector<std::shared_ptr<Timestamp>> Timeline; 

     

        std::map<UnresolvedTagPlacement, std::vector<std::wstring>> UnresolvedTags;
     
        ///<summary>Absolute URI for the TS file</summary>
        std::wstring MediaUri;
        ///<summary>Duration in ticks</summary>
        unsigned long long Duration;
        ///<summary>Cumulative (from the start of the presentation) duration in ticks</summary>
        unsigned long long CumulativeDuration;
       
        ///<summary>Is segment a byte range ?</summary>
        bool IsHttpByteRange;
        ///<summary>TCE to signal segment processing completion</summary>
        task_completion_event<tuple<HRESULT, unsigned int>> tceSegmentProcessingCompleted;
        ///<summary>Offset (in bytes) from the beginning of the playlist</summary>
        unsigned long long ByteRangeOffset;
        ///<summary>Length of the segment in bytes</summary>
        unsigned int LengthInBytes;
        ///<summary>Encryption key needed for this segment</summary>
        shared_ptr<EncryptionKey> EncKey;
        shared_ptr<MediaSegment> spCloaking;
        bool Discontinous;
        bool StartsDiscontinuity; 
        bool IsTransportStream;
        //Sequence number
        unsigned int SequenceNumber;

        std::shared_ptr<Timestamp> ProgramDateTime;
        ///<summary>MediaSegment constructor</summary>
        ///<param name='tagWithAttributes'>The EXTINF tag string with attributes</param>
        ///<param name='mediauri'>The URL to the segment resource</param>
        ///<param name='parentplaylist'>The parent playlist (variant - not master)</param>
        ///<param name='byterangeinfo'>The applicable EXT-X-BYTERANGE tag data in case the segment is a byte range</param>
        MediaSegment(std::wstring& tagWithAttributes, Playlist *parentplaylist);

        shared_ptr<Timestamp> GetFirstMinPTS();

        void SetUri(std::wstring& mediauri);
        void SetByteRangeInfo(const std::wstring& byterangeinfo);
        bool SetPIDFilter(shared_ptr<std::map<ContentType, unsigned short>> pidfilter = nullptr); 
        std::map<ContentType, unsigned short> GetPIDFilter();
        bool ResetPIDFilter(ContentType forType);
        bool ResetPIDFilter();
       
        ///<summary>Downloads and parses transport stream segment</summary>
        task<HRESULT> DownloadSegmentDataAsync(task_completion_event<HRESULT> tceSegmentDownloadCompleted = task_completion_event<HRESULT>());

        HRESULT OnSegmentDownloadCompleted(CHLSMediaSource* ms, 
          Microsoft::HLSClient::IHLSContentDownloader^ downloader, 
          Microsoft::HLSClient::IHLSContentDownloadCompletedArgs ^args, 
          task_completion_event<HRESULT> tceSegmentDownloadCompleted);

        

        /* shared_ptr<MediaSegment> NeedCloakingForPendingBitrateShift();
         HRESULT AttemptCloakingForPendingBitrateShift(shared_ptr<MediaSegment> targetSeg, CHLSMediaSource *ms, shared_ptr<CDownloader> spDownloader, task_completion_event<HRESULT> tceSegmentDownloadCompleted);*/
        HRESULT AttemptBitrateShiftOnStreamThinning(CHLSMediaSource *ms, DefaultContentDownloader^ spDownloader, task_completion_event<HRESULT> tceSegmentDownloadCompleted);
        HRESULT AttemptBitrateShiftOnDownloadFailure(CHLSMediaSource *ms, DefaultContentDownloader^ pDownloader, task_completion_event<HRESULT> tceSegmentDownloadCompleted);
        ///<summary>Gets the current state of the segment</summary>
        ///<returns>Current segment state</returns>
        MediaSegmentState GetCurrentState();

        ///<summary>Sets the current state on a media segment</summary>
        ///<param name='state'>The new state</param>
        void SetCurrentState(MediaSegmentState state);
        
        void UpdateSampleDiscontinuityTimestamps(shared_ptr<MediaSegment> prevplayedseg,bool IgnoreUnreadSamples = false);
        void UpdateSampleDiscontinuityTimestamps(shared_ptr<SampleData> lastvidsample, shared_ptr<SampleData> lastaudsample);
        void UpdateSampleDiscontinuityTimestamps(unsigned long long lastts);
        
        void CancelDownloads(bool WaitForRunningTasks = false);
        ///<summary>Gets the MPEG2 TS PID for a given media type</summary>
        ///<param name='type'>The media type to look for</param>
        ///<returns>Program ID</returns>
        unsigned short GetPIDForMediaType(ContentType type);
        shared_ptr<SampleData> GetLastSample(ContentType type,bool IgnoreUnread = false);
        shared_ptr<SampleData> GetLastSample(bool IgnoreUnread = false);
        ///<summary>Checks to see if all streams within the segment are at an end i.e. the segment has been fully read</summary>
        ///<returns>True or False</returns>
        bool IsReadEOS();

        bool CanScavenge();
        ///<summary>Checks to see if a stream within the segment is at an end</summary>
        ///<param name='PID'>The PID for the stream</param>
        ///<returns>True or False</returns>
        bool IsReadEOS(unsigned short PID);

        unsigned int SampleReadCount();
        ///<summary>Checks if the segment has a program of a given media type</summary>
        ///<param name='type'>The media type to look for</param>
        ///<returns>true if found, false otherwise</returns>
        bool HasMediaType(ContentType type);

        ///<summary>Gets the next unread sample for a program</summary>
        ///<param name='PID'>The PID for the program in which we are looking for a sample</param>
        ///<returns>MF Sample</returns>
        void GetNextSample(unsigned short PID, MFRATE_DIRECTION Direction, IMFSample **ppSample);
        shared_ptr<Timestamp> PositionQueueAtNextIDR(unsigned short PID, MFRATE_DIRECTION Direction, unsigned short IDRSkipCount = 0);
        ///<summary>Checks to see if a segment includes a given time point</summary>
        ///<param name='TimePoint'>The time point to check for (in ticks and offset from the beginning of the presentation)</param>
        ///<returns>True if the segment contains the time point, false otherwise</returns>
        bool IncludesTimePoint(unsigned long long TimePoint, MFRATE_DIRECTION Direction = MFRATE_FORWARD); 

        ///<summary>Finds next nearest keyframe sample for a specific program at a given timestamp</summary>
        ///<param name='Timepoint'>The time stamp at which we are trying to find a sample</param>
        ///<param name='PID'>The PID for the program</param>
        ///<param name='Direction'>Forward or Reverse</param>
        ///<returns>Returns a the SampleData instance for a matching sample if found, or nullptr if not</returns>   
        shared_ptr<SampleData> FindNearestIDRSample(unsigned long long Timepoint, unsigned short PID, 
          MFRATE_DIRECTION Direction = MFRATE_FORWARD, bool IsTimepointDiscontinous = false, unsigned short differenceType = 0);

        shared_ptr<SampleData> PeekNextIDR(MFRATE_DIRECTION Direction, unsigned short IDRSkipCount);

        ///<summary>Rewinds a segment(all unread sample queues for each PID) fully</summary>
        void RewindInCacheSegment();
        void RewindInCacheSegment(unsigned short PID);

        ///<summary>Gets the current position within a segment for a specific PID</summary>
        ///<returns>The current position in ticks</returns> 
        unsigned long long GetCurrentPosition(unsigned short PID, MFRATE_DIRECTION direction);

        ///<summary>Attempts to set the current position withn a segment for a specific PID</summary>
        ///<param name='PID'>PID to set the position for</param>
        ///<param name='PosInTicks'>The normalized position (in ticks)</param>
        ///<returns>The actual timepoints that the sample queue for the supplied PID got set to</returns>
        ///<remarks>If there is not an exact match to the supplied timestamp in a specific sample queue, the queue gets 
        ///set to the nearest timestamp immediately after the supplied position</remarks>
        unsigned long long SetCurrentPosition(unsigned short PID, unsigned long long PosInTicks, MFRATE_DIRECTION direction, bool IsPositionAbsolute = false);
        unsigned long long SetCurrentPosition(unsigned short PID, unsigned long long PosInTicks,TimestampMatch Match, MFRATE_DIRECTION direction, bool IsPositionAbsolute = false);

        ///<summary>Gets the current position within a segment</summary>
        ///<returns>The current position in ticks</returns>
        ///<remarks>Returns the maximum of current positions across all PIDs</remarks>
        unsigned long long GetCurrentPosition(MFRATE_DIRECTION direction);

        ///<summary>Attempts to set the current position withn a segment(all PIDs)</summary>
        ///<param name='PosInTicks'>The normalized position (in ticks)</param>
        ///<returns>A map keyed by content type, with values being the actual timepoints that the sample queue for that content type got set to</returns>
        ///<remarks>If there is not an exact match to the supplied timestamp in a specific sample queue, the queue gets 
        ///set to the nearest timestamp immediately after the supplied position</remarks>
        std::map<ContentType, unsigned long long> SetCurrentPosition(unsigned long long PosInTicks, MFRATE_DIRECTION direction, bool IsPositionAbsolute = false);
        std::map<ContentType, unsigned long long> SetCurrentPosition(unsigned long long PosInTicks, TimestampMatch Match, MFRATE_DIRECTION direction, bool IsPositionAbsolute = false);

        ///<summary>Normalizes a timestamp</summary>
        ///<param name='ts'>The timestamp to normalize</param>
        ///<returns>The normalized timestamp</returns>
        ///<remarks> Normalization is the process of reducing the start timestamp of a presentation (not just a segment) to 0, 
        //and then offsetting every timestamp that follows by the original value of the start timestamp. This gives us a timeline
        ///that begins with a tiemstamp = 0</remarks>
        shared_ptr<Timestamp> TSAbsoluteToRelative(shared_ptr<Timestamp> ts);

        ///<summary>Normalizes a timestamp</summary>
        ///<param name='ts'>The timestamp to normalize</param>
        ///<returns>The normalized timestamp</returns>
        ///<remarks> Normalization is the process of reducing the start timestamp of a presentation (not just a segment) to 0, 
        //and then offsetting every timestamp that follows by the original value of the start timestamp. This gives us a timeline
        ///that begins with a tiemstamp = 0</remarks>
        shared_ptr<Timestamp> TSAbsoluteToRelative(unsigned long long tsval);

        ///<summary>Denormalizes a timestamp</summary>
        ///<param name='ts'>The timestamp to denormalize</param>
        ///<returns>The denormalized timestamp</returns>
        ///<remarks> Denormalization is the process of taking a normalized timestamp and converting it to its original value</remarks>
        shared_ptr<Timestamp> TSRelativeToAbsolute(shared_ptr<Timestamp> ts);

        ///<summary>Denormalizes a timestamp</summary>
        ///<param name='ts'>The timestamp to denormalize</param>
        ///<returns>The denormalized timestamp</returns>
        ///<remarks> Denormalization is the process of taking a normalized timestamp and converting it to its original value</remarks>
        shared_ptr<Timestamp> TSRelativeToAbsolute(unsigned long long tsval);

        ///<summary>Returns the sequence number of the current segment relative to a zero base(i.e. offset by the playlist base sequence number i.e. EXT-X-MEDIA-SEQUENCE)</summary>
        ///<remarks>We want a zero based sequence number to stay in tune with zero based collection indexing. If the sequence number of the first segment is non zero, this returns the current sequence number minus the base sequence number for the playlist.</remarks>
        ///<returns>Sequence Number</returns>
        //unsigned int GetSequenceNumberZeroBased();

        ///<summary>Calculates the time duration left in a segment from a given time point</summary>
        ///<param name='Position'>The timepoint to calculate duration from (in ticks)</param>
        ///<returns>The remaining duration (in ticks)</returns>
        unsigned long long GetSegmentLookahead(unsigned long long Position, MFRATE_DIRECTION direction);

        ///<summary>Calculates the time duration left in a segment from current position of the read head on the segment</summary> 
        ///<returns>The remaining duration (in ticks)</returns>
        unsigned long long GetSegmentLookahead(MFRATE_DIRECTION direction);

        ///<summary>Sets the start and end Program timsetamps for the segment</summary>
        void SetPTSBoundaries();

        //void PrepareForThinning();
        bool HasUnreadCCData();

        ///<summary>Advances the queue of unread samples for a specific PID to a specific timestamp</summary>
        ///<param name='PID'>The PID of the stream whose sample queue to advance</param>
        ///<param name='toPTS'>The timestamp to forward to</param>
        ///<param name='Exact'>If set to true, only forwards if an exact timestamp match is found. If set to false, 
        ///and no matching timestamp is found, forwards to the timestamp that is immediatel after the specified time point</param>

        void AdvanceUnreadQueue(unsigned short PID, shared_ptr<Timestamp> toPTS);

        void AdvanceUnreadQueue(unsigned short PID, shared_ptr<Timestamp> toPTS, TimestampMatch Match);

        ///<summary>Rewinds the queue of unread samples for a specific PID to a specific timestamp</summary>
        ///<param name='PID'>The PID of the stream whose sample queue to rewind</param>
        ///<param name='toPTS'>The timestamp to rewind to</param>
        ///<param name='Exact'>If set to true, only rewinds if an exact timestamp match is found. If set to false, 
        ///and no matching timestamp is found, rewinds to the timestamp that is immediately after the specified time point</param>

        void RewindUnreadQueue(unsigned short PID, shared_ptr<Timestamp> toPTS);

        void RewindUnreadQueue(unsigned short PID, shared_ptr<Timestamp> toPTS, TimestampMatch Match);
 
        ///<summary>MediaSegment destructor<//summary>
        ~MediaSegment();


        ///<summary>Clear data</summary>
        void Scavenge(bool Force = false);

        unsigned long long GetApproximateFrameDistance(ContentType type, unsigned short tgtPID);
        
        unsigned int GetSequenceNumber()
        {
           
          return SequenceNumber;
        }
        ///<summary>Set the sequence number</summary>
        void SetSequenceNumber(unsigned int seqNum)
        {
          std::lock_guard<std::recursive_mutex> lock(LockSegment);
          SequenceNumber = seqNum;
        }


        shared_ptr<MediaSegment> GetCloaking() {
          
          return spCloaking;
        };
        void SetCloaking(shared_ptr<MediaSegment> seg) {
          std::lock_guard<std::recursive_mutex> lock(LockSegment);
          spCloaking = seg;
        }
        ///<summary>Checks to see if there are any samples to read</summary>
        ///<param name='PID'>The PID of the stream to check</param>
        ///<returns>True or False</returns>
        bool IsEmptySampleQueue(unsigned short PID)
        {
          return UnreadQueues[PID].empty();
        }

        ///<summary>Reads the next unread sample without removing it from the queue</summary>
        ///<param name='PID'>The PID of the stream to read</param>
        ///<returns>The next sample</returns>
        std::shared_ptr<SampleData> PeekNextSample(unsigned short PID, MFRATE_DIRECTION direction)
        {
          std::shared_ptr<SampleData> ret = nullptr;
          ret = UnreadQueues[PID].empty() == false ? (direction == MFRATE_DIRECTION::MFRATE_FORWARD ? UnreadQueues[PID].front() : UnreadQueues[PID].back()) : nullptr;
          return ret;
        }
        void AssociateChain()
        {
          InterlockedIncrement16(&chainAssociationCount);
        }
        short GetChainAssociation()
        {
          return chainAssociationCount;
        }
        void ResetChainAssociation()
        {
          InterlockedExchange16(&chainAssociationCount, 0);
        }

        unsigned long long TSDiscontinousRelativeToAbsolute(unsigned long long ts, shared_ptr<MediaSegment> prevSegment = nullptr);
        unsigned long long TSAbsoluteToDiscontinousRelative(unsigned long long ts, shared_ptr<MediaSegment> prevSegment = nullptr);

       

        unsigned long long TSAbsoluteToDiscontinousAbsolute(unsigned long long ts);
        unsigned long long TSDiscontinousAbsoluteToAbsolute(unsigned long long ts);

        vector<unsigned int> FailedCloaking;

        void AddFailedCloaking(unsigned int br)
        {
          std::lock_guard<std::recursive_mutex> lock(LockSegment);
          if (std::find(begin(FailedCloaking), end(FailedCloaking), br) == FailedCloaking.end())
            FailedCloaking.push_back(br);
        }

        void ResetFailedCloaking()
        {
          std::lock_guard<std::recursive_mutex> lock(LockSegment);
          FailedCloaking.clear();
        }

        bool HasCloakingFailedAt(unsigned int br)
        {
          std::lock_guard<std::recursive_mutex> lock(LockSegment);
          return !(std::find(begin(FailedCloaking), end(FailedCloaking), br) == FailedCloaking.end());
        }
      };

    }
  }
} 