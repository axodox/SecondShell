#pragma once

namespace SecondShell::Session
{
  //Text that must not linger in memory, such as a password: it cannot be copied, and it is overwritten when destroyed.
  class Secret
  {
  public:
    Secret() = default;
    explicit Secret(std::wstring_view value);
    ~Secret();

    Secret(Secret&& other) noexcept = default;
    Secret& operator=(Secret&& other) noexcept;
    Secret(const Secret&) = delete;
    Secret& operator=(const Secret&) = delete;

    std::wstring_view View() const;

  private:
    std::vector<wchar_t> _value;

    void Clear();
  };

  //What signs the user in to the child session.
  struct Credentials
  {
    std::wstring UserName;  //with its domain, e.g. "PC\user"
    Secret Password;
  };
}
