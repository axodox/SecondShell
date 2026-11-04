#include "pch.h"
#include "ActionValues.h"

using namespace Axodox::Json;
using namespace std;

namespace SecondShell::Actions
{
  json_object_descriptor<Size> Size::json_description = describe_json_object<Size>({
    { &Size::Width, "width", { .is_required = true, .schema = { .minimum = 1 } } },
    { &Size::Height, "height", { .is_required = true, .schema = { .minimum = 1 } } }
    });

  json_object_descriptor<ClipboardContent> ClipboardContent::json_description = describe_json_object<ClipboardContent>({
    { &ClipboardContent::Format, "format", { .is_required = true } },
    { &ClipboardContent::Data, "data", { .is_required = false, .schema = { .description = "text and html: a string; image and raw: base64; files: an array of paths." } } },
    { &ClipboardContent::Path, "path", { .is_required = false, .schema = { .description = "image: an image file (PNG, JPEG, BMP, ...) to put on the clipboard instead of data." } } },
    { &ClipboardContent::Name, "name", { .is_required = false, .schema = { .description = "raw: a registered clipboard format name, a standard one such as CF_UNICODETEXT, or a format number." } } }
    });

  json_object_descriptor<InputState> InputState::json_description = describe_json_object<InputState>({
    { &InputState::MouseButtons, "mouseButtons", { .is_required = false } },
    { &InputState::Keys, "keys", { .is_required = false } },
    { &InputState::TouchPoints, "touchPoints", { .is_required = false } },
    { &InputState::Pen, "pen", { .is_required = false, .schema = { .description = "True if the pen was touching the screen." } } }
    });

  json_object_descriptor<CapturedImage> CapturedImage::json_description = describe_json_object<CapturedImage>({
    { &CapturedImage::Width, "width" },
    { &CapturedImage::Height, "height" },
    { &CapturedImage::Scale, "scale", { .schema = { .description = "Image pixels per session pixel: divide image coordinates by it to get session pixels." } } },
    { &CapturedImage::Format, "format" },
    { &CapturedImage::Data, "data", json_base64_converter{}, { .is_required = false } },
    { &CapturedImage::Path, "path", { .is_required = false } }
    });

  json_object_descriptor<ProcessInfo> ProcessInfo::json_description = describe_json_object<ProcessInfo>({
    { &ProcessInfo::ProcessId, "pid" },
    { &ProcessInfo::ParentProcessId, "parentPid" },
    { &ProcessInfo::Name, "name" },
    { &ProcessInfo::ExecutablePath, "path", { .is_required = false } },
    { &ProcessInfo::CpuPercent, "cpuPercent", { .is_required = false } },
    { &ProcessInfo::MemoryBytes, "memoryBytes" }
    });

  bool WindowQuery::IsEmpty() const
  {
    return !Window && !Title && !ProcessName && !ProcessId;
  }

  Axodox::Infrastructure::expected<> WindowQuery::Validate(bool isRequired) const
  {
    if (Window && (Title || ProcessName || ProcessId)) return Axodox::Infrastructure::failure<>("give either window, or any of title, processName and pid, not both.");
    if (isRequired && IsEmpty()) return Axodox::Infrastructure::failure<>("which window? Give window (an ID from Window.Enumerate), or any of title, processName and pid.");
    return {};
  }

  string WindowQuery::to_string() const
  {
    if (Window) return format("window {}", Window->to_string());

    vector<string> parts;
    if (Title) parts.push_back(format("title '{}'", *Title));
    if (ProcessName) parts.push_back(format("process {}", *ProcessName));
    if (ProcessId) parts.push_back(format("pid {}", *ProcessId));

    string result;
    for (size_t index = 0; index < parts.size(); index++)
    {
      if (index > 0) result += index + 1 == parts.size() ? " and " : ", ";
      result += parts[index];
    }
    return result.empty() ? "the active window" : result;
  }

  json_object_descriptor<WindowInfo> WindowInfo::json_description = describe_json_object<WindowInfo>({
    { &WindowInfo::Window, "window" },
    { &WindowInfo::Title, "title" },
    { &WindowInfo::ProcessId, "pid" },
    { &WindowInfo::ProcessName, "processName" },
    { &WindowInfo::X, "x" },
    { &WindowInfo::Y, "y" },
    { &WindowInfo::Width, "width" },
    { &WindowInfo::Height, "height" },
    { &WindowInfo::State, "state" },
    { &WindowInfo::IsForeground, "isForeground" }
    });
}
