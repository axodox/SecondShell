#include "pch.h"
#include "AutomationTextSource.h"

using namespace Axodox::Infrastructure;
using namespace SecondShell::Actions;
using namespace std;

namespace SecondShell::Capture
{
  //Long enough for a busy application, short enough not to stall a wait's checks on a hung one
  const DWORD _connectionTimeout = 500;
  const DWORD _transactionTimeout = 1000;

  namespace
  {
    using unique_bstr = unique_ptr<OLECHAR, decltype(&SysFreeString)>;
    using unique_safearray = unique_ptr<SAFEARRAY, decltype(&SafeArrayDestroy)>;

    string ToString(BSTR text)
    {
      return text ? winrt::to_string(wstring_view{ text, SysStringLen(text) }) : string{};
    }

    winrt::com_ptr<IUIAutomation2> CreateAutomation()
    {
      auto automation = winrt::create_instance<IUIAutomation2>(__uuidof(CUIAutomation8), CLSCTX_INPROC_SERVER);
      winrt::check_hresult(automation->put_ConnectionTimeout(_connectionTimeout));
      winrt::check_hresult(automation->put_TransactionTimeout(_transactionTimeout));
      return automation;
    }

    winrt::com_ptr<IUIAutomationCacheRequest> CreateRequest(IUIAutomation* automation)
    {
      winrt::com_ptr<IUIAutomationCacheRequest> request;
      winrt::check_hresult(automation->CreateCacheRequest(request.put()));
      for (auto property : { UIA_NamePropertyId, UIA_ValueValuePropertyId, UIA_BoundingRectanglePropertyId, UIA_IsTextPatternAvailablePropertyId })
      {
        winrt::check_hresult(request->AddProperty(property));
      }
      winrt::check_hresult(request->AddPattern(UIA_TextPatternId));
      return request;
    }

    //The control view's elements which are not scrolled or hidden away
    winrt::com_ptr<IUIAutomationCondition> CreateShownCondition(IUIAutomation* automation)
    {
      winrt::com_ptr<IUIAutomationCondition> controlView;
      winrt::check_hresult(automation->get_ControlViewCondition(controlView.put()));

      VARIANT isOffscreen;
      isOffscreen.vt = VT_BOOL;
      isOffscreen.boolVal = VARIANT_FALSE;
      winrt::com_ptr<IUIAutomationCondition> onScreen;
      winrt::check_hresult(automation->CreatePropertyCondition(UIA_IsOffscreenPropertyId, isOffscreen, onScreen.put()));

      winrt::com_ptr<IUIAutomationCondition> condition;
      winrt::check_hresult(automation->CreateAndCondition(controlView.get(), onScreen.get(), condition.put()));
      return condition;
    }

    //The rectangles are left, top, width and height, one after the other
    Rect Union(SAFEARRAY* rectangles)
    {
      LONG lower = 0, upper = -1;
      SafeArrayGetLBound(rectangles, 1, &lower);
      SafeArrayGetUBound(rectangles, 1, &upper);

      double* values = nullptr;
      winrt::check_hresult(SafeArrayAccessData(rectangles, reinterpret_cast<void**>(&values)));

      auto left = numeric_limits<double>::max(), top = left, right = numeric_limits<double>::lowest(), bottom = right;
      for (auto index = 0; index + 4 <= upper - lower + 1; index += 4)
      {
        left = min(left, values[index]);
        top = min(top, values[index + 1]);
        right = max(right, values[index] + values[index + 2]);
        bottom = max(bottom, values[index + 1] + values[index + 3]);
      }
      SafeArrayUnaccessData(rectangles);

      if (right <= left || bottom <= top) return {};
      return { int32_t(floor(left)), int32_t(floor(top)), uint32_t(ceil(right - floor(left))), uint32_t(ceil(bottom - floor(top))) };
    }
  }

  AutomationTextSource::AutomationTextSource() :
    _automation(CreateAutomation()),
    _request(CreateRequest(_automation.get())),
    _shownElements(CreateShownCondition(_automation.get()))
  { }

  vector<ScreenWord> AutomationTextSource::Read(WindowId window) const
  {
    try
    {
      return ReadElements(reinterpret_cast<HWND>(window.Handle));
    }
    catch (const winrt::hresult_error& error)
    {
      //E.g. a timeout, a closed window, or an elevated one (UIPI): OCR reads it
      _logger.log(log_severity::debug, "No text from window {}: {}", window.to_string(), winrt::to_string(error.message()));
      return {};
    }
  }

  vector<ScreenWord> AutomationTextSource::ReadElements(HWND window) const
  {
    winrt::com_ptr<IUIAutomationElement> root;
    winrt::check_hresult(_automation->ElementFromHandleBuildCache(window, _request.get(), root.put()));

    winrt::com_ptr<IUIAutomationElementArray> elements;
    winrt::check_hresult(root->FindAllBuildCache(TreeScope_Subtree, _shownElements.get(), _request.get(), elements.put()));

    vector<ScreenWord> words;
    auto count = 0;
    if (elements) winrt::check_hresult(elements->get_Length(&count));
    for (auto index = 0; index < count; index++)
    {
      winrt::com_ptr<IUIAutomationElement> element;
      winrt::check_hresult(elements->GetElement(index, element.put()));

      if (HasTextPattern(element.get()))
      {
        AddShownText(element.get(), words);
      }
      else
      {
        AddNameAndValue(element.get(), words);
      }
    }
    return words;
  }

  bool AutomationTextSource::HasTextPattern(IUIAutomationElement* element)
  {
    VARIANT value;
    VariantInit(&value);
    winrt::check_hresult(element->GetCachedPropertyValue(UIA_IsTextPatternAvailablePropertyId, &value));
    auto result = value.vt == VT_BOOL && value.boolVal == VARIANT_TRUE;
    VariantClear(&value);
    return result;
  }

  void AutomationTextSource::AddShownText(IUIAutomationElement* element, vector<ScreenWord>& words)
  {
    winrt::com_ptr<IUIAutomationTextPattern> pattern;
    winrt::check_hresult(element->GetCachedPatternAs(UIA_TextPatternId, IID_PPV_ARGS(pattern.put())));

    winrt::com_ptr<IUIAutomationTextRangeArray> ranges;
    winrt::check_hresult(pattern->GetVisibleRanges(ranges.put()));

    auto count = 0;
    winrt::check_hresult(ranges->get_Length(&count));
    for (auto index = 0; index < count; index++)
    {
      winrt::com_ptr<IUIAutomationTextRange> range;
      winrt::check_hresult(ranges->GetElement(index, range.put()));

      BSTR text = nullptr;
      winrt::check_hresult(range->GetText(-1, &text));
      unique_bstr textOwner{ text, &SysFreeString };

      SAFEARRAY* rectangles = nullptr;
      winrt::check_hresult(range->GetBoundingRectangles(&rectangles));
      unique_safearray rectanglesOwner{ rectangles, &SafeArrayDestroy };

      //One run per visible range: the range's lines have boxes, but not always one per line of its text
      auto box = rectangles ? Union(rectangles) : Rect{};
      if (!box.IsEmpty()) words.push_back({ ToString(text), box });
    }
  }

  void AutomationTextSource::AddNameAndValue(IUIAutomationElement* element, vector<ScreenWord>& words)
  {
    RECT bounds;
    winrt::check_hresult(element->get_CachedBoundingRectangle(&bounds));
    Rect box{ bounds.left, bounds.top, uint32_t(max(bounds.right - bounds.left, LONG(0))), uint32_t(max(bounds.bottom - bounds.top, LONG(0))) };
    if (box.IsEmpty()) return;

    BSTR name = nullptr;
    winrt::check_hresult(element->get_CachedName(&name));
    unique_bstr nameOwner{ name, &SysFreeString };
    auto nameText = ToString(name);
    if (!nameText.empty()) words.push_back({ nameText, box });

    VARIANT value;
    VariantInit(&value);
    winrt::check_hresult(element->GetCachedPropertyValue(UIA_ValueValuePropertyId, &value));
    auto valueText = value.vt == VT_BSTR ? ToString(value.bstrVal) : string{};
    VariantClear(&value);
    if (!valueText.empty() && valueText != nameText) words.push_back({ valueText, box });
  }
}
