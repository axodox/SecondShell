#pragma once

namespace SecondShell::Input
{
  class IKeyboardInjector
  {
  public:
    virtual ~IKeyboardInjector() = default;

    virtual void Press(uint16_t virtualKey) = 0;
    virtual void Release(uint16_t virtualKey) = 0;

    //Unicode input, independent of the keyboard layout.
    virtual void TypeCharacter(char32_t character) = 0;
  };
}
