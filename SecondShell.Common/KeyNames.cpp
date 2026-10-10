#include "pch.h"
#include "KeyNames.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Json;
using namespace std;

namespace SecondShell::Actions
{
  namespace
  {
    struct KeyName
    {
      const char* Name;
      uint16_t Code;
    };

    //The names of System.Windows.Forms.Keys with their virtual key codes; for codes with several names, the first is the canonical one.
    const KeyName _keyNames[] = {
      { "LButton", 0x01 },
      { "RButton", 0x02 },
      { "Cancel", 0x03 },
      { "MButton", 0x04 },
      { "XButton1", 0x05 },
      { "XButton2", 0x06 },
      { "Back", 0x08 },
      { "Tab", 0x09 },
      { "LineFeed", 0x0A },
      { "Clear", 0x0C },
      { "Enter", 0x0D },
      { "Return", 0x0D },
      { "ShiftKey", 0x10 },
      { "ControlKey", 0x11 },
      { "Menu", 0x12 },
      { "Pause", 0x13 },
      { "CapsLock", 0x14 },
      { "Capital", 0x14 },
      { "KanaMode", 0x15 },
      { "HangulMode", 0x15 },
      { "HanguelMode", 0x15 },
      { "JunjaMode", 0x17 },
      { "FinalMode", 0x18 },
      { "KanjiMode", 0x19 },
      { "HanjaMode", 0x19 },
      { "Escape", 0x1B },
      { "IMEConvert", 0x1C },
      { "IMENonconvert", 0x1D },
      { "IMEAccept", 0x1E },
      { "IMEAceept", 0x1E },
      { "IMEModeChange", 0x1F },
      { "Space", 0x20 },
      { "PageUp", 0x21 },
      { "Prior", 0x21 },
      { "PageDown", 0x22 },
      { "Next", 0x22 },
      { "End", 0x23 },
      { "Home", 0x24 },
      { "Left", 0x25 },
      { "Up", 0x26 },
      { "Right", 0x27 },
      { "Down", 0x28 },
      { "Select", 0x29 },
      { "Print", 0x2A },
      { "Execute", 0x2B },
      { "PrintScreen", 0x2C },
      { "Snapshot", 0x2C },
      { "Insert", 0x2D },
      { "Delete", 0x2E },
      { "Help", 0x2F },
      { "D0", 0x30 },
      { "D1", 0x31 },
      { "D2", 0x32 },
      { "D3", 0x33 },
      { "D4", 0x34 },
      { "D5", 0x35 },
      { "D6", 0x36 },
      { "D7", 0x37 },
      { "D8", 0x38 },
      { "D9", 0x39 },
      { "A", 0x41 },
      { "B", 0x42 },
      { "C", 0x43 },
      { "D", 0x44 },
      { "E", 0x45 },
      { "F", 0x46 },
      { "G", 0x47 },
      { "H", 0x48 },
      { "I", 0x49 },
      { "J", 0x4A },
      { "K", 0x4B },
      { "L", 0x4C },
      { "M", 0x4D },
      { "N", 0x4E },
      { "O", 0x4F },
      { "P", 0x50 },
      { "Q", 0x51 },
      { "R", 0x52 },
      { "S", 0x53 },
      { "T", 0x54 },
      { "U", 0x55 },
      { "V", 0x56 },
      { "W", 0x57 },
      { "X", 0x58 },
      { "Y", 0x59 },
      { "Z", 0x5A },
      { "LWin", 0x5B },
      { "RWin", 0x5C },
      { "Apps", 0x5D },
      { "Sleep", 0x5F },
      { "NumPad0", 0x60 },
      { "NumPad1", 0x61 },
      { "NumPad2", 0x62 },
      { "NumPad3", 0x63 },
      { "NumPad4", 0x64 },
      { "NumPad5", 0x65 },
      { "NumPad6", 0x66 },
      { "NumPad7", 0x67 },
      { "NumPad8", 0x68 },
      { "NumPad9", 0x69 },
      { "Multiply", 0x6A },
      { "Add", 0x6B },
      { "Separator", 0x6C },
      { "Subtract", 0x6D },
      { "Decimal", 0x6E },
      { "Divide", 0x6F },
      { "F1", 0x70 },
      { "F2", 0x71 },
      { "F3", 0x72 },
      { "F4", 0x73 },
      { "F5", 0x74 },
      { "F6", 0x75 },
      { "F7", 0x76 },
      { "F8", 0x77 },
      { "F9", 0x78 },
      { "F10", 0x79 },
      { "F11", 0x7A },
      { "F12", 0x7B },
      { "F13", 0x7C },
      { "F14", 0x7D },
      { "F15", 0x7E },
      { "F16", 0x7F },
      { "F17", 0x80 },
      { "F18", 0x81 },
      { "F19", 0x82 },
      { "F20", 0x83 },
      { "F21", 0x84 },
      { "F22", 0x85 },
      { "F23", 0x86 },
      { "F24", 0x87 },
      { "NumLock", 0x90 },
      { "Scroll", 0x91 },
      { "LShiftKey", 0xA0 },
      { "RShiftKey", 0xA1 },
      { "LControlKey", 0xA2 },
      { "RControlKey", 0xA3 },
      { "LMenu", 0xA4 },
      { "RMenu", 0xA5 },
      { "BrowserBack", 0xA6 },
      { "BrowserForward", 0xA7 },
      { "BrowserRefresh", 0xA8 },
      { "BrowserStop", 0xA9 },
      { "BrowserSearch", 0xAA },
      { "BrowserFavorites", 0xAB },
      { "BrowserHome", 0xAC },
      { "VolumeMute", 0xAD },
      { "VolumeDown", 0xAE },
      { "VolumeUp", 0xAF },
      { "MediaNextTrack", 0xB0 },
      { "MediaPreviousTrack", 0xB1 },
      { "MediaStop", 0xB2 },
      { "MediaPlayPause", 0xB3 },
      { "LaunchMail", 0xB4 },
      { "SelectMedia", 0xB5 },
      { "LaunchApplication1", 0xB6 },
      { "LaunchApplication2", 0xB7 },
      { "OemSemicolon", 0xBA },
      { "Oem1", 0xBA },
      { "Oemplus", 0xBB },
      { "Oemcomma", 0xBC },
      { "OemMinus", 0xBD },
      { "OemPeriod", 0xBE },
      { "OemQuestion", 0xBF },
      { "Oem2", 0xBF },
      { "Oemtilde", 0xC0 },
      { "Oem3", 0xC0 },
      { "OemOpenBrackets", 0xDB },
      { "Oem4", 0xDB },
      { "OemPipe", 0xDC },
      { "Oem5", 0xDC },
      { "OemCloseBrackets", 0xDD },
      { "Oem6", 0xDD },
      { "OemQuotes", 0xDE },
      { "Oem7", 0xDE },
      { "Oem8", 0xDF },
      { "OemBackslash", 0xE2 },
      { "Oem102", 0xE2 },
      { "ProcessKey", 0xE5 },
      { "Packet", 0xE7 },
      { "Attn", 0xF6 },
      { "Crsel", 0xF7 },
      { "Exsel", 0xF8 },
      { "EraseEof", 0xF9 },
      { "Play", 0xFA },
      { "Zoom", 0xFB },
      { "NoName", 0xFC },
      { "Pa1", 0xFD },
      { "OemClear", 0xFE }
    };

    //Aliases models tend to use
    const KeyName _aliases[] = {
      { "Ctrl", 0x11 },
      { "Control", 0x11 },
      { "Alt", 0x12 },
      { "Shift", 0x10 },
      { "Win", 0x5B },
      { "Esc", 0x1B },
      { "Backspace", 0x08 },
      { "0", 0x30 }, { "1", 0x31 }, { "2", 0x32 }, { "3", 0x33 }, { "4", 0x34 },
      { "5", 0x35 }, { "6", 0x36 }, { "7", 0x37 }, { "8", 0x38 }, { "9", 0x39 }
    };

    const char* const _keyDescription = "A key name of the .NET System.Windows.Forms.Keys enumeration (case-insensitive, e.g. Enter, Escape, Tab, F5, A, D1, NumPad1, Left, PageDown, LWin), an alias (Ctrl, Alt, Shift, Win, Esc, Backspace, 0-9), or a virtual key code as a number.";
  }

  optional<uint16_t> KeyNames::TryParse(string_view name)
  {
    auto key = to_lower(trim(name));
    for (auto& names : { span<const KeyName>(_keyNames), span<const KeyName>(_aliases) })
    {
      for (auto& item : names)
      {
        if (to_lower(item.Name) == key) return item.Code;
      }
    }

    return nullopt;
  }

  string KeyNames::ToString(uint16_t key)
  {
    for (auto& item : _keyNames)
    {
      if (item.Code == key) return item.Name;
    }

    return std::to_string(key);
  }

  value_ptr<json_value> KeySchema::to_json() const
  {
    auto result = make_value<json_object>();
    result->set_value("description", description ? description : _keyDescription);
    string_view anyOf = R"([{"type":"string"},{"type":"integer","minimum":1,"maximum":254}])";
    result->set_value("anyOf", json_value::from_string(anyOf));
    return result;
  }

  expected<> KeySchema::validate_value(const json_value& value) const
  {
    uint16_t key;
    if (KeyConverter::from_json(&value, key)) return {};
    return failure<>("Must be a key name, e.g. Enter, A or F5, or a virtual key code between 1 and 254.");
  }

  value_ptr<json_value> KeyConverter::to_json(uint16_t key)
  {
    return make_value<json_string>(KeyNames::ToString(key));
  }

  bool KeyConverter::from_json(const json_value* json, uint16_t& key)
  {
    if (!json) return false;

    if (auto number = json->try_as<json_number>())
    {
      if (number->value < 1 || number->value > 254 || number->value != floor(number->value)) return false;
      key = uint16_t(number->value);
      return true;
    }

    if (auto text = json->try_as<json_string>())
    {
      auto result = KeyNames::TryParse(text->value);
      if (!result) return false;
      key = *result;
      return true;
    }

    return false;
  }

  bool KeyConverter::is_default(uint16_t key)
  {
    return key == 0;
  }
}