#pragma once
#include "ActionValues.h"
#include "ClipboardManager.h"
#include "MessageWindow.h"

namespace SecondShell::Desktop
{
  //One side of the clipboard sharing in interactive mode (Design.md, Clipboard management): while watching, reports this
  //session's clipboard changes as raw content in each of its formats, and writes what the other side reports. Its own
  //writes are not reported back, so the two sides do not echo each other.
  class SECONDSHELL_COMMON_API ClipboardMirror
  {
    inline static const Axodox::Infrastructure::logger _logger{ "ClipboardMirror" };
    Axodox::Infrastructure::event_owner _events;

  public:
    explicit ClipboardMirror(Axodox::Infrastructure::dependency_container* container);
    ~ClipboardMirror();

    void IsWatching(bool value);

    std::vector<Actions::ClipboardContent> Read();
    void Write(const std::vector<Actions::ClipboardContent>& contents);

    //Raised on the clipboard's window thread.
    Axodox::Infrastructure::event_publisher<ClipboardMirror*, const std::vector<Actions::ClipboardContent>&> Changed;

  private:
    std::shared_ptr<MessageWindow> _window;
    std::shared_ptr<ClipboardManager> _clipboard;
    Axodox::Infrastructure::event_subscription _messageReceivedSubscription;
    std::atomic_bool _isWatching = false;
    std::atomic<DWORD> _writtenSequence = 0;

    std::vector<uint32_t> FormatsToShare();
    void OnMessageReceived(MessageWindow* sender, UINT message);
  };
}
