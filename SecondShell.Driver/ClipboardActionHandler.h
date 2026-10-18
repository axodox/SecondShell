#pragma once
#include "ActionResult.h"
#include "CancellationToken.h"
#include "CaptureEncoder.h"
#include "ClipboardActions.h"
#include "ClipboardFormatConverter.h"
#include "ClipboardFormats.h"
#include "ClipboardManager.h"

namespace SecondShell::Driver
{
  //Performs the Clipboard.* actions. As with captures, the host decides where an image goes and saves files.
  class ClipboardActionHandler
  {
  public:
    explicit ClipboardActionHandler(Axodox::Infrastructure::dependency_container* container);

    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Execute(const Actions::Action& action, const Rpc::CancellationToken& cancellation);

  private:
    std::shared_ptr<Desktop::ClipboardManager> _clipboard;
    std::shared_ptr<Desktop::ClipboardFormatConverter> _converter;
    std::shared_ptr<Capture::CaptureEncoder> _encoder;
    uint32_t _htmlFormat;
    uint32_t _pngFormat;

    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Read(const Actions::ClipboardReadAction& action);
    Axodox::Infrastructure::value_ptr<Actions::ActionResult> Write(const Actions::ClipboardWriteAction& action);

    //Fills in the content or image; false if the clipboard has nothing in that format.
    bool ReadContent(const Actions::ClipboardReadAction& action, Actions::ClipboardResult& result);
    std::optional<Actions::CapturedImage> ReadImage(const std::optional<Actions::Size>& resolution);
    std::vector<Desktop::ClipboardData> ToClipboardData(const Actions::ClipboardContent& content) const;
    uint32_t RawFormat(const std::optional<std::string>& name) const;
  };
}
