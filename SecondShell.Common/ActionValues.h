#pragma once
#include "ActionEnums.h"
#include "WindowId.h"

//Values used by several actions and results.
namespace SecondShell::Actions
{
  struct SECONDSHELL_COMMON_API Size
  {
    static Axodox::Json::json_object_descriptor<Size> json_description;

    uint32_t Width = 0;
    uint32_t Height = 0;
  };

  //Clipboard content in one format: text and html are strings, image is base64 PNG (or a file path), files is an
  //array of paths, raw is base64 bytes of the clipboard format given by name or number.
  struct SECONDSHELL_COMMON_API ClipboardContent
  {
    static Axodox::Json::json_object_descriptor<ClipboardContent> json_description;

    ClipboardFormat Format = ClipboardFormat::Text;
    Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> Data;
    std::optional<std::string> Path;
    std::optional<std::string> Name;
  };

  //What was held down: reported when a sequence stops early, after it was released.
  struct SECONDSHELL_COMMON_API InputState
  {
    static Axodox::Json::json_object_descriptor<InputState> json_description;

    std::vector<MouseButton> MouseButtons;
    std::vector<std::string> Keys;
    std::vector<uint32_t> TouchPoints;
  };

  //An image of a capture or of the clipboard.
  struct SECONDSHELL_COMMON_API CapturedImage
  {
    static Axodox::Json::json_object_descriptor<CapturedImage> json_description;

    uint32_t Width = 0;
    uint32_t Height = 0;
    double Scale = 1.0;  //image pixels per session pixel
    ImageFormat Format = ImageFormat::Png;
    std::vector<uint8_t> Data;
    std::optional<std::string> Path;
  };

  struct SECONDSHELL_COMMON_API ProcessInfo
  {
    static Axodox::Json::json_object_descriptor<ProcessInfo> json_description;

    uint32_t ProcessId = 0;
    uint32_t ParentProcessId = 0;
    std::string Name;
    std::optional<std::string> ExecutablePath;
    std::optional<double> CpuPercent;
    uint64_t MemoryBytes = 0;
  };

  struct SECONDSHELL_COMMON_API WindowInfo
  {
    static Axodox::Json::json_object_descriptor<WindowInfo> json_description;

    WindowId Window;
    std::string Title;
    uint32_t ProcessId = 0;
    std::string ProcessName;
    int32_t X = 0;
    int32_t Y = 0;
    uint32_t Width = 0;
    uint32_t Height = 0;
    WindowState State = WindowState::Normal;
    bool IsForeground = false;
  };
}
