#include "pch.h"
#include "Credentials.h"

using namespace std;

namespace SecondShell::Session
{
  Secret::Secret(wstring_view value) :
    _value(value.begin(), value.end())
  { }

  Secret::~Secret()
  {
    Clear();
  }

  Secret& Secret::operator=(Secret&& other) noexcept
  {
    if (this != &other)
    {
      Clear();
      _value = move(other._value);
    }

    return *this;
  }

  wstring_view Secret::View() const
  {
    return { _value.data(), _value.size() };
  }

  void Secret::Clear()
  {
    SecureZeroMemory(_value.data(), _value.size() * sizeof(wchar_t));
    _value.clear();
  }
}
