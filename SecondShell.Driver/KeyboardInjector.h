#pragma once
#include "IKeyboardInjector.h"
#include "InputInjectorProvider.h"

namespace SecondShell::Input
{
  class KeyboardInjector : public IKeyboardInjector
  {
  public:
    explicit KeyboardInjector(Axodox::Infrastructure::dependency_container* container);

    virtual void Press(uint16_t virtualKey) override;
    virtual void Release(uint16_t virtualKey) override;
    virtual void TypeCharacter(char32_t character) override;

  private:
    std::shared_ptr<InputInjectorProvider> _provider;

    void InjectKey(uint16_t virtualKey, bool isUp);
    void InjectUnicode(wchar_t codeUnit);
  };
}
