#include "pch.h"
#include "XamlRowLogic.h"

using namespace Microsoft::CC608;

using namespace Platform;
using namespace Windows::UI;
using namespace Windows::UI::Text;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Documents;

using namespace std;

XamlRowLogic::XamlRowLogic(void) : _videoHeightPixels(0), Options(nullptr)
{
}

XamlRowLogic::~XamlRowLogic(void)
{
}

Windows::UI::Xaml::UIElement^ XamlRowLogic::RenderRow(const Microsoft::CC608::MemoryRow& row)
{
  if (!row.ContainsText())
  {
    return CreateGenericTextBlock();
  }

  // if we make it this far, we need to bite the bullet and build all necessary objects
  InitializeForNonEmptyRow();

  for(const auto& cell : row.Cells)
  {
    if (cell.Attributes.ContainsAttributes())
    {
      // assume new styles to render
      CreateAndCloseExistingObject();
      LoadNewStyles(cell);
    }

    if (_currentHasBlackBackground && cell.IsTransparentSpaceOrNullChar())
    {
      // need to stop with the background
      CreateAndCloseExistingObject();
      LoadNewStyles(cell);
    }

    if (!_currentHasBlackBackground && !cell.IsTransparentSpaceOrNullChar())
    {
      // need to start with the background
      CreateAndCloseExistingObject();
      LoadNewStyles(cell);
    }

    AddCurrentCharacter(cell);
  }

  CreateAndCloseExistingObject();

  return _rowStackPanel;
}

void XamlRowLogic::CreateAndCloseExistingObject()
{
	if (!_charactersHaveBeenProcessed)
	{
		// don't add an empty element if we haven't processed anything yet
		return;
	}

	if (!_currentHasBlackBackground)
	{
		CreateInvisibleTextBlock(_characters.str());

		// clear the characters
		_characters.str(L"");
		return;
	}

	TextBlock^ tb = CreateGenericTextBlock();

	if (_currentUnderline)
	{
		Underline^ underline = ref new Underline();
		Run^ run = ref new Run();
		run->Text = ref new String(_characters.str().c_str());

		underline->Inlines->Append(run);

		tb->Inlines->Append(underline);
	}
	else
	{
		Run^ run = ref new Run();
		run->Text = ref new String(_characters.str().c_str());

		tb->Inlines->Append(run);
	}

	_characters.str(L"");

	if (_currentItalic)
	{
		tb->FontStyle = FontStyle::Italic;
	}

	if (_currentFlash)
	{
		// not implemented
	}

	if ( Options != nullptr && Options->FontBrush)
	{
		tb->Foreground = Options->FontBrush;
	}
	else
	{
		switch (_currentColor)
		{
		case CC608CharColor::White:
			tb->Foreground = ref new SolidColorBrush(Windows::UI::Colors::White);
			break;

		case CC608CharColor::Green:
			tb->Foreground = ref new SolidColorBrush(Windows::UI::Colors::Green);
			break;

		case CC608CharColor::Blue:
			tb->Foreground = ref new SolidColorBrush(Windows::UI::Colors::Blue);
			break;

		case CC608CharColor::Cyan:
			tb->Foreground = ref new SolidColorBrush(Windows::UI::Colors::Cyan);
			break;

		case CC608CharColor::Red:
			tb->Foreground = ref new SolidColorBrush(Windows::UI::Colors::Red);
			break;

		case CC608CharColor::Yellow:
			tb->Foreground = ref new SolidColorBrush(Windows::UI::Colors::Yellow);
			break;

		case CC608CharColor::Magenta:
			tb->Foreground = ref new SolidColorBrush(Windows::UI::Colors::Magenta);
			break;

		default:
			tb->Foreground = ref new SolidColorBrush(Windows::UI::Colors::White);  // default to white
			break;
		}
	}

	// add the black background
	Border^ background = ref new Border();

	background->Margin = Thickness(5.0, 0, 5.0, 0);

	if (Options != nullptr && Options->BackgroundBrush)
	{
		background->Background = Options->BackgroundBrush;
	}
	else
	{
		background->Background = ref new SolidColorBrush(Windows::UI::Colors::Black);
	}

	background->Child = tb;

  _rowStackPanel->Children->Append(background);
}


void XamlRowLogic::LoadNewStyles(const MemoryCell& cell)
{
  _currentColor = cell.Attributes.GetColor();
  _currentFlash = cell.Attributes.IsFlash();
  _currentItalic = cell.Attributes.IsItalics();
  _currentUnderline = cell.Attributes.IsUnderline();

  _currentHasBlackBackground = !cell.IsTransparentSpaceOrNullChar();
}

void XamlRowLogic::AddCurrentCharacter(const MemoryCell& cell)
{
  if (cell.IsTransparentSpaceOrNullChar() || cell.Character == L' ')
  {
    // inject a XAML non-breaking space (decimal character code 160)
    _characters << L'\x00A0';
    _charactersHaveBeenProcessed = true;
    return;
  }

  _characters << cell.Character;
  _charactersHaveBeenProcessed = true;
}

void XamlRowLogic::SetVideoHeight(const uint16 pixels)
{
  _videoHeightPixels = pixels;

	//_captionsBlockHeight = pixels * 0.80f; // 80% of screen height reserved for captions
  if (this->Options != nullptr) {
    switch (Options->FontSize)
    {
    case 50:
      _captionsBlockHeight = pixels * 0.60f;
      break;
    case 100:
      _captionsBlockHeight = pixels * 0.80f;
      break;

    case 150:
      _captionsBlockHeight = pixels * 0.90f;
      break;

    case 200:
      _captionsBlockHeight = pixels;
      break;
    }
  }
  else
    _captionsBlockHeight = pixels * 0.80f;

  _lineHeight = _captionsBlockHeight / 15.0f; // each line is 1/15 of the available caption height
  _fontSize = _lineHeight / 1.3f;
}

void XamlRowLogic::InitializeForNonEmptyRow()
{
  assert(_videoHeightPixels != 0);

  // set defaults for styles
  _currentColor = CC608CharColor::White;
  _currentFlash = false;
  _currentItalic = false;
  _currentUnderline = false;
  _currentHasBlackBackground = false;

  // setup the stack panel to hold whatever the row contains
  _rowStackPanel = ref new StackPanel();
  _rowStackPanel->Orientation = Orientation::Horizontal;

  _charactersHaveBeenProcessed = false;
}

TextBlock^ XamlRowLogic::CreateGenericTextBlock()
{
  TextBlock^ textBlock = ref new TextBlock();
  
  textBlock->LineHeight = _lineHeight;
  textBlock->FontSize = _fontSize;

  if (this->Options == nullptr || this->Options->FontFamily->Length() == 0)
  {
	  textBlock->FontFamily = ref new FontFamily(L"Consolas, Courier New, Courier");
  }
  else
  {
	  textBlock->FontFamily = ref new FontFamily(this->Options->FontFamily);
  }

  textBlock->LineStackingStrategy = LineStackingStrategy::BlockLineHeight;

  if (this->Options != nullptr && this->Options->IsSmallCaps)
  {
	  Windows::UI::Xaml::Documents::Typography::SetCapitals(textBlock, Windows::UI::Xaml::FontCapitals::SmallCaps);
  }

  textBlock->Margin = Thickness(5.0, 0, 5.0, 0);

  return textBlock;
}

void XamlRowLogic::CreateInvisibleTextBlock(std::wstring content)
{
  TextBlock^ textBlock = CreateGenericTextBlock();

  textBlock->Text = ref new Platform::String(content.c_str());

  _rowStackPanel->Children->Append(textBlock);
}
