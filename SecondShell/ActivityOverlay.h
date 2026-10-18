#pragma once
#include "OverlayElement.h"

namespace SecondShell::Rendering
{
  //What the agent types, the keys it presses, the commands it runs and its clipboard actions: a list in the bottom left
  //corner of the session, the newest at the bottom, the older ones scrolling up and fading out. Held keys stay
  //highlighted until released. Typed sensitive text is hidden, and long texts are truncated (Design.md, Security).
  class ActivityOverlay : public OverlayElement
  {
    using time_point = std::chrono::steady_clock::time_point;

  public:
    ActivityOverlay();

    virtual void OnEvent(const Agent::VisualEvent& event) override;
    virtual bool Draw(ID2D1DeviceContext* context, const ViewTransform& transform, time_point now) override;

  private:
    struct Entry
    {
      uint64_t Id = 0;
      std::wstring Tag;
      std::wstring Text;
      time_point Added;
      time_point Shown;                 //when it last changed
      std::optional<uint16_t> HeldKey;  //while held
      winrt::com_ptr<IDWriteTextLayout> Layout;
    };

    winrt::com_ptr<IDWriteFactory> _writeFactory;
    winrt::com_ptr<IDWriteTextFormat> _format;
    std::deque<Entry> _entries;

    void OnStarted(const Agent::VisualEvent& event);
    void OnFinished(const Agent::VisualEvent& event);

    void Add(const Agent::VisualEvent& event, std::wstring_view tag, std::wstring text, std::optional<uint16_t> heldKey = {});
    void Update(uint64_t id, std::wstring_view text, time_point time);
    void ReleaseKey(std::optional<uint16_t> key, time_point time);

    IDWriteTextLayout* Layout(Entry& entry) const;
    void DrawEntry(ID2D1DeviceContext* context, Entry& entry, D2D1_POINT_2F bottomLeft, float opacity) const;
  };
}
