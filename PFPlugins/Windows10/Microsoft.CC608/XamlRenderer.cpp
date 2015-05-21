#include "pch.h"

#include "XamlRenderer.h"


using namespace Microsoft::CC608;

using namespace Windows::Foundation;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Media::Animation;


XamlRenderer::XamlRenderer(void) : _rowLogic()
{
}


XamlRenderer::~XamlRenderer(void)
{
}


UIElement^ XamlRenderer::RenderXaml(const Memory& displayMemory,
  const unsigned short videoHeightPixels,
  const bool animateUp)
{
  _rowLogic.SetVideoHeight(videoHeightPixels);

  if (animateUp)
  {
    // build animated captions that roll up

    float captionBlockHeight = videoHeightPixels * 0.80f;

    if (_rowLogic.Options != nullptr) {
      switch (_rowLogic.Options->FontSize)
      {
      case 50:
        captionBlockHeight = videoHeightPixels * 0.60f;
        break;

      case 150:
        captionBlockHeight = videoHeightPixels * 0.90f;
        break;

      case 200:
        captionBlockHeight = videoHeightPixels;
        break;
      }
    }


    Grid^ rollUpContainer = ref new Grid();

    rollUpContainer->Tag = "Rollup Container";

    if (_rowLogic.Options != nullptr && _rowLogic.Options->CaptionWidth > 0)
    {
      rollUpContainer->MinWidth = _rowLogic.Options->CaptionWidth;
      rollUpContainer->HorizontalAlignment = HorizontalAlignment::Left;
      auto leftMargin = (_rowLogic.Options->VideoWidth - _rowLogic.Options->CaptionWidth) / 2.0;
      rollUpContainer->Margin = Thickness(leftMargin, 0, 0, 0);
    }
    else
    {
      rollUpContainer->HorizontalAlignment = HorizontalAlignment::Center;
    }

    rollUpContainer->VerticalAlignment = VerticalAlignment::Center;

    StackPanel^ stackPanel = ref new StackPanel();

    if (_rowLogic.Options != nullptr && _rowLogic.Options->WindowBrush != nullptr)
    {
      stackPanel->Background = _rowLogic.Options->WindowBrush;
    }
    //stackPanel->Margin = Thickness(5.0);
    stackPanel->Orientation = Orientation::Vertical;
    Point p;
    p.X = 0.5;
    p.Y = 0.5;
    stackPanel->RenderTransformOrigin = p;

    CompositeTransform^ transform = ref new CompositeTransform();
    transform->TranslateY = captionBlockHeight / 15.0f;
    stackPanel->RenderTransform = transform;

    DoubleAnimation^ animation = ref new DoubleAnimation();
    TimeSpan durationTimeSpan;
    durationTimeSpan.Duration = 2500000; // 0.25 s
    animation->Duration = durationTimeSpan;
    animation->To = 0.0;

    Storyboard^ storyboard = ref new Storyboard();
    storyboard->Children->Append(animation);
    storyboard->SetTarget(animation, stackPanel);
    storyboard->SetTargetProperty(animation, "(UIElement.RenderTransform).(CompositeTransform.TranslateY)");

    for (const MemoryRow& row : displayMemory.Rows)
    {
      stackPanel->Children->Append(_rowLogic.RenderRow(row));
    }

    rollUpContainer->Children->Append(stackPanel);

    // this is a bit of a hack--start the animation now
    // (Otherwise, if we try to wire up to the loading event of the StackPanel, we could end up creating a circular dependency chain with the storyboard, leading to a memory leak)
    storyboard->Begin();

    return rollUpContainer;
  }
  else
  {
    // if no animations, just render the simple object tree to speed up display
    StackPanel^ stackPanel = ref new StackPanel();

    stackPanel->Tag = "StackPanel";

    if (_rowLogic.Options != nullptr && _rowLogic.Options->WindowBrush)
    {
      stackPanel->Background = _rowLogic.Options->WindowBrush;
    }

    if (_rowLogic.Options != nullptr && _rowLogic.Options->CaptionWidth > 0)
    {
      stackPanel->MinWidth = _rowLogic.Options->CaptionWidth;
      stackPanel->HorizontalAlignment = HorizontalAlignment::Left;
      auto leftMargin = (_rowLogic.Options->VideoWidth - _rowLogic.Options->CaptionWidth) / 2.0;
      stackPanel->Margin = Thickness(leftMargin, 0, 0, 0);
    }
    else
    {
      stackPanel->HorizontalAlignment = HorizontalAlignment::Center;
    }

    stackPanel->Orientation = Orientation::Vertical;
    stackPanel->VerticalAlignment = VerticalAlignment::Center;

    for (const MemoryRow& row : displayMemory.Rows)
    {
      stackPanel->Children->Append(_rowLogic.RenderRow(row));
    }

    return stackPanel;
  }
}


void XamlRenderer::SetOptions(CaptionOptions^ options)
{
  _rowLogic.Options = options;
}