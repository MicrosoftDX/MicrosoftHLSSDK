#pragma once
#include "pch.h"

#include <sstream>
#include "CC608CharColor.h"
#include "MemoryRow.h"
#include "MemoryCell.h"
#include "CaptionOptions.h"

namespace Microsoft { namespace CC608 {

// Testable class to render a row of captions into XAML
// If the row is empty, it renders a single TextBlock; otherwise, it creates a horizontal StackPanel
// and adds necessary elements to it.
class XamlRowLogic
{
public:
  XamlRowLogic(void);
  ~XamlRowLogic(void);

  Windows::UI::Xaml::UIElement^ RenderRow(const Microsoft::CC608::MemoryRow& row);

  void SetVideoHeight(const uint16 pixels);

  // <summary>the caption options</summary>
  CaptionOptions^ Options;

private:
  // holds all the UIElements for the row
  Windows::UI::Xaml::Controls::StackPanel^ _rowStackPanel;
  
  Windows::UI::Xaml::Controls::TextBlock^ CreateGenericTextBlock();

  void CreateInvisibleTextBlock(std::wstring content);

  void CreateAndCloseExistingObject();
  void AddCurrentCharacter(const MemoryCell& cell);
  void LoadNewStyles(const MemoryCell& cell);

  // tracks if any characters have been processed (so we don't emit an empty TextBlock at the start of the row)
  bool _charactersHaveBeenProcessed;

  // holds all characters in the current style before they are pushed into objects
  std::wstringstream _characters;

  // styles change as the cells are iterated over
  CC608CharColor _currentColor;
  bool _currentItalic;
  bool _currentFlash;
  bool _currentUnderline;
  bool _currentHasBlackBackground;

  uint16 _videoHeightPixels;
  float _captionsBlockHeight;
  float _fontSize;
  float _lineHeight;

  void InitializeForNonEmptyRow();
};

}}