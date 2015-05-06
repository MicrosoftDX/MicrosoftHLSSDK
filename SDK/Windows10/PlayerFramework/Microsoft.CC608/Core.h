#pragma once

#include <vector>
#include <mutex>
#include <memory>

#include "pch.h"
#include "HtmlRenderer.h"
#include "XamlRenderer.h"
#include "CaptionDataQueue.h"
#include "HtmlCaptionsData.h"
#include "XamlCaptionsData.h"
#include "Model.h"
#include "ByteDecoder.h"
#include "Timestamp.h"

namespace Microsoft { namespace CC608 {

// Class to orchestrate all the modules and the high-level activity of the 608 component
class Core
{
public:
  Core();
  ~Core(void);

  void AddCaptionData(const Timestamp ts, const std::vector<byte_t>& bytes);
  void AddCaptionData(const std::vector<std::pair<Timestamp, std::vector<byte_t>>>& data);

  void AddCaptionDataInUserDataEnvelope(const Timestamp ts, const std::vector<byte_t>& bytes);
  void AddCaptionDataInUserDataEnvelope(const std::vector<std::pair<Timestamp, std::vector<byte_t>>>& data);

  // seek to a new position (behavior depends upon streaming mode--if streaming, delete all queue data, but either way change the timestamp in the model)
  void Seek(const Timestamp seekLocation);
  
  // Clears model, data queue, and all settings
  // call this when PlayloadUpdated (as opposed to PlayloadAugmented) is fired, or when media source changes
  void Reset();

  void AdvanceModelTo(const Timestamp newPosition);

  // returns true if the displayed memory has changed since it was last rendered
  bool NeedsUpdate() const;

  std::wstring GetCurrentHtml(const unsigned short videoHeightPixels);

  Windows::UI::Xaml::UIElement^ GetCurrentXaml(const unsigned short videoHeightPixels);

  // valid values are 1, 2, 3, and 4 (for CC1, CC2, CC3, and CC4), and 0 for no captions
  int GetCaptionTrack() { return _captionTrack; }
  void SetCaptionTrack(const int captionTrack);

  template<typename T>
  std::vector<T> ConvertIVector(Windows::Foundation::Collections::IVector<T>^ v)
  {
    vector<T> result;

    for (int i = 0; i != v->Size; ++i)
    {
      result.push_back(v->GetAt(i));
    }

    return result;
  }

  CaptionOptions^ Options;

private:
  CaptionDataQueue _queue;

  std::unique_ptr<Microsoft::CC608::ByteDecoder> _upDecoder;
  HtmlRenderer _htmlRenderer;
  XamlRenderer _xamlRenderer;

  std::shared_ptr<Microsoft::CC608::Model> _spModel;

  // the timestamp for the caption state that the model currently represents
  Timestamp _currentPosition;

  unsigned int _previousDisplayVersionNumber;
  int _captionTrack;

  void LogRawData(const std::pair<Timestamp, std::vector<byte_t>>& data) const;
};


}}