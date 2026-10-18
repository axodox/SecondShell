#pragma once
#include "MessageWindow.h"

namespace SecondShell::Desktop
{
  //The data of one clipboard format.
  struct SECONDSHELL_COMMON_API ClipboardData
  {
    uint32_t Format = 0;
    std::vector<uint8_t> Bytes;
  };

  //The clipboard of this process's session, in Windows clipboard formats; works on the message window's thread, which
  //owns what it writes. Every session has a clipboard of its own.
  class SECONDSHELL_COMMON_API ClipboardManager
  {
  public:
    explicit ClipboardManager(Axodox::Infrastructure::dependency_container* container);

    std::vector<uint32_t> Formats();

    //The format's data, nullopt if the clipboard has none; throws length_error above the limit.
    std::optional<std::vector<uint8_t>> Read(uint32_t format, size_t limit);

    //Replaces the clipboard's content.
    void Write(std::span<const ClipboardData> data);

  private:
    std::shared_ptr<MessageWindow> _window;

    //Opens the clipboard on the window's thread, retrying while another application has it open.
    void WithClipboard(const std::function<void()>& action);
  };
}
