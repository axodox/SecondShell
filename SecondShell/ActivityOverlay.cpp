#include "pch.h"
#include "ActivityOverlay.h"
#include "ClipboardActions.h"
#include "KeyboardActions.h"
#include "KeyNames.h"
#include "ProcessActions.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Json;
using namespace SecondShell::Actions;
using namespace SecondShell::Agent;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Rendering
{
  const size_t _maxEntries = 8;
  const size_t _maxTextLength = 60;
  const milliseconds _shownDuration{ 4000 };
  const milliseconds _fadeDuration{ 500 };
  const milliseconds _slideDuration{ 200 };

  //In device independent pixels
  const float _fontSize = 13.f;
  const float _margin = 12.f;
  const float _entryHeight = 24.f;
  const float _entryGap = 4.f;
  const float _padding = 8.f;
  const float _cornerRadius = 4.f;

  const D2D1_COLOR_F _background{ 0.f, 0.f, 0.f, 0.65f };
  const D2D1_COLOR_F _heldBackground{ 0.369f, 0.294f, 0.839f, 0.9f };
  const D2D1_COLOR_F _textColor{ 1.f, 1.f, 1.f, 1.f };

  namespace
  {
    //One line, at most _maxTextLength characters
    wstring Shorten(string_view text)
    {
      wstring result;
      for (auto character : to_wstring(text))
      {
        if (character == L'\r') continue;
        if (character == L'\n') result += L'↵';
        else result += character < L' ' ? L' ' : character;
      }

      if (result.size() <= _maxTextLength) return result;

      result.resize(_maxTextLength - 1);
      if (IS_HIGH_SURROGATE(result.back())) result.pop_back();
      return result + L'…';
    }

    wstring KeysText(const vector<KeyModifier>& modifiers, uint16_t key)
    {
      string text;
      for (auto modifier : modifiers) text += format("{}+", to_string(modifier));
      return Shorten(text + KeyNames::ToString(key));
    }

    wstring TypedText(const KeyboardTypeAction& action)
    {
      if (action.Sensitive.value_or(false)) return wstring(min(action.Text.size(), size_t(12)), L'•');
      return Shorten(action.Text);
    }

    wstring CommandText(const ProcessRunAction& action)
    {
      auto text = action.Command;
      for (auto& argument : action.Args) text += " " + argument;
      return Shorten(text);
    }

    wstring ContentText(const ClipboardContent& content)
    {
      auto isString = content.Data && content.Data->type() == json_type::string;
      switch (content.Format)
      {
      case ClipboardFormat::Text:
        return isString ? Shorten(content.Data->as<json_string>().value) : L"text";
      case ClipboardFormat::Html:
        return L"HTML";
      case ClipboardFormat::Image:
        return L"image";
      case ClipboardFormat::Files:
      {
        auto count = content.Data && content.Data->type() == json_type::array ? content.Data->as<json_array>().value.size() : 0;
        return format(L"{} file{}", count, count == 1 ? L"" : L"s");
      }
      default:
        return Shorten(content.Name.value_or("data"));
      }
    }

    wstring ProcessText(const ProcessResult& result)
    {
      auto text = format(L"PID {}", result.ProcessId);
      if (result.ExitCode) text += format(L", exit code {}", *result.ExitCode);
      return text;
    }
  }

  ActivityOverlay::ActivityOverlay()
  {
    winrt::check_hresult(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(_writeFactory.put())));
    winrt::check_hresult(_writeFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, _fontSize, L"", _format.put()));
    winrt::check_hresult(_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));
    winrt::check_hresult(_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER));
  }

  void ActivityOverlay::OnEvent(const VisualEvent& event)
  {
    if (event.Result) OnFinished(event);
    else OnStarted(event);
  }

  void ActivityOverlay::OnStarted(const VisualEvent& event)
  {
    auto& action = event.Action;
    switch (action.Type())
    {
    case ActionType::KeyboardType:
      Add(event, L"Type", TypedText(static_cast<const KeyboardTypeAction&>(action)));
      break;
    case ActionType::KeyboardTap:
    {
      auto& tap = static_cast<const KeyboardTapAction&>(action);
      Add(event, L"Key", KeysText(tap.Modifiers, tap.Key));
      break;
    }
    case ActionType::KeyboardPress:
    {
      auto& press = static_cast<const KeyboardPressAction&>(action);
      Add(event, L"Hold", KeysText(press.Modifiers, press.Key), press.Key);
      break;
    }
    case ActionType::KeyboardRelease:
      ReleaseKey(static_cast<const KeyboardReleaseAction&>(action).Key, event.Time);
      break;
    case ActionType::KeyboardReset:
    case ActionType::ControlReset:
      ReleaseKey(nullopt, event.Time);
      break;
    case ActionType::ProcessRun:
      Add(event, L"Run", CommandText(static_cast<const ProcessRunAction&>(action)));
      break;
    case ActionType::ProcessStop:
      Add(event, L"Stop", format(L"PID {}", static_cast<const ProcessStopAction&>(action).ProcessId));
      break;
    case ActionType::ProcessKill:
    {
      auto& kill = static_cast<const ProcessKillAction&>(action);
      Add(event, L"Kill", format(L"PID {}{}", kill.ProcessId, kill.Tree.value_or(false) ? L" and its children" : L""));
      break;
    }
    case ActionType::ClipboardWrite:
    {
      auto& write = static_cast<const ClipboardWriteAction&>(action);
      if (!write.Content.empty()) Add(event, L"Clipboard", L"set " + ContentText(write.Content.front()));
      break;
    }
    }
  }

  void ActivityOverlay::OnFinished(const VisualEvent& event)
  {
    auto& result = *event.Result;
    switch (event.Action.Type())
    {
    case ActionType::ProcessRun:
    {
      auto process = dynamic_cast<const ProcessResult*>(&result);
      if (process && process->ProcessId) Update(event.Id, ProcessText(*process), event.Time);
      else if (result.Status != ActionStatus::Succeeded) Update(event.Id, L"failed", event.Time);
      break;
    }
    case ActionType::ClipboardRead:
    {
      auto clipboard = dynamic_cast<const ClipboardResult*>(&result);
      if (!clipboard) break;

      if (clipboard->Image) Add(event, L"Clipboard", format(L"read image {}×{}", clipboard->Image->Width, clipboard->Image->Height));
      else if (clipboard->Content) Add(event, L"Clipboard", L"read " + ContentText(*clipboard->Content));
      break;
    }
    }
  }

  void ActivityOverlay::Add(const VisualEvent& event, wstring_view tag, wstring text, optional<uint16_t> heldKey)
  {
    _entries.push_back({ event.Id, wstring(tag), move(text), event.Time, event.Time, heldKey });
    if (_entries.size() > _maxEntries) _entries.pop_front();
  }

  void ActivityOverlay::Update(uint64_t id, wstring_view text, time_point time)
  {
    auto entry = ranges::find(_entries, id, &Entry::Id);
    if (entry == _entries.end()) return;

    entry->Text += format(L" · {}", text);
    entry->Shown = time;
    entry->Layout = nullptr;
  }

  void ActivityOverlay::ReleaseKey(optional<uint16_t> key, time_point time)
  {
    for (auto& entry : _entries)
    {
      if (!entry.HeldKey || (key && entry.HeldKey != key)) continue;

      entry.HeldKey.reset();
      entry.Shown = time;
    }
  }

  bool ActivityOverlay::Draw(ID2D1DeviceContext* context, const ViewTransform& transform, time_point now)
  {
    erase_if(_entries, [&](const Entry& entry) { return !entry.HeldKey && now - entry.Shown >= _shownDuration + _fadeDuration; });
    if (_entries.empty()) return false;

    //In device independent pixels from here
    D2D1_MATRIX_3X2_F viewTransform;
    context->GetTransform(&viewTransform);
    context->SetTransform(D2D1::Matrix3x2F::Scale(transform.UiScale, transform.UiScale) * viewTransform);

    //A new entry pushes the others up as it slides in
    auto slide = 1.f - EaseOut(Progress(_entries.back().Added, _slideDuration, now));
    auto step = _entryHeight + _entryGap;
    D2D1_POINT_2F bottomLeft{ transform.Session.left / transform.UiScale + _margin, transform.Session.bottom / transform.UiScale - _margin + slide * step };

    auto isAnimating = slide > 0.f;
    for (auto entry = _entries.rbegin(); entry != _entries.rend(); entry++)
    {
      auto opacity = entry->HeldKey ? 1.f : 1.f - Progress(entry->Shown + _shownDuration, _fadeDuration, now);
      if (entry == _entries.rbegin()) opacity *= 1.f - slide;

      DrawEntry(context, *entry, bottomLeft, opacity);
      bottomLeft.y -= step;
      isAnimating |= !entry->HeldKey;
    }

    context->SetTransform(viewTransform);
    return isAnimating;
  }

  IDWriteTextLayout* ActivityOverlay::Layout(Entry& entry) const
  {
    if (entry.Layout) return entry.Layout.get();

    auto text = entry.Tag + L"  " + entry.Text;
    winrt::check_hresult(_writeFactory->CreateTextLayout(text.c_str(), uint32_t(text.size()), _format.get(), 10000.f, _entryHeight, entry.Layout.put()));
    winrt::check_hresult(entry.Layout->SetFontWeight(DWRITE_FONT_WEIGHT_SEMI_BOLD, { 0, uint32_t(entry.Tag.size()) }));
    return entry.Layout.get();
  }

  void ActivityOverlay::DrawEntry(ID2D1DeviceContext* context, Entry& entry, D2D1_POINT_2F bottomLeft, float opacity) const
  {
    auto layout = Layout(entry);
    DWRITE_TEXT_METRICS metrics;
    winrt::check_hresult(layout->GetMetrics(&metrics));

    D2D1_RECT_F card{ bottomLeft.x, bottomLeft.y - _entryHeight, bottomLeft.x + metrics.widthIncludingTrailingWhitespace + 2 * _padding, bottomLeft.y };

    winrt::com_ptr<ID2D1SolidColorBrush> background;
    winrt::check_hresult(context->CreateSolidColorBrush(entry.HeldKey ? _heldBackground : _background, background.put()));
    background->SetOpacity(opacity);
    context->FillRoundedRectangle({ card, _cornerRadius, _cornerRadius }, background.get());

    winrt::com_ptr<ID2D1SolidColorBrush> text;
    winrt::check_hresult(context->CreateSolidColorBrush(_textColor, text.put()));
    text->SetOpacity(opacity);
    context->DrawTextLayout({ card.left + _padding, card.top }, layout, text.get());
  }
}
