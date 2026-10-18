#include "pch.h"
#include "ClipboardManager.h"

using namespace Axodox::Infrastructure;
using namespace std;
using namespace std::chrono;

namespace SecondShell::Desktop
{
  //Applications keep the clipboard open only for a moment
  const milliseconds _openTimeout{ 2000 };
  const milliseconds _openRetryInterval{ 20 };

  namespace
  {
    struct ClipboardCloser
    {
      ~ClipboardCloser()
      {
        CloseClipboard();
      }
    };

    struct LockedMemory
    {
      HGLOBAL Memory;
      void* Data;

      explicit LockedMemory(HGLOBAL memory) :
        Memory(memory),
        Data(::GlobalLock(memory))
      {
        if (!Data) throw runtime_error("The clipboard data could not be read.");
      }

      ~LockedMemory()
      {
        GlobalUnlock(Memory);
      }
    };

    HGLOBAL ToGlobal(span<const uint8_t> bytes)
    {
      auto memory = GlobalAlloc(GMEM_MOVEABLE, max(bytes.size(), size_t(1)));
      if (!memory) throw bad_alloc();

      {
        LockedMemory lock{ memory };
        memcpy(lock.Data, bytes.data(), bytes.size());
      }
      return memory;
    }
  }

  ClipboardManager::ClipboardManager(dependency_container* container) :
    _window(container->resolve<MessageWindow>())
  { }

  vector<uint32_t> ClipboardManager::Formats()
  {
    vector<uint32_t> formats;
    WithClipboard([&] {
      for (auto format = EnumClipboardFormats(0); format; format = EnumClipboardFormats(format)) formats.push_back(format);
      });
    return formats;
  }

  optional<vector<uint8_t>> ClipboardManager::Read(uint32_t format, size_t limit)
  {
    optional<vector<uint8_t>> data;
    WithClipboard([&] {
      auto memory = HGLOBAL(GetClipboardData(format));
      if (!memory) return;

      auto size = GlobalSize(memory);
      if (size > limit) throw length_error(std::format("The clipboard data is {} bytes, more than the limit of {}.", size, limit));

      LockedMemory lock{ memory };
      data.emplace(static_cast<const uint8_t*>(lock.Data), static_cast<const uint8_t*>(lock.Data) + size);
      });
    return data;
  }

  void ClipboardManager::Write(span<const ClipboardData> data)
  {
    WithClipboard([&] {
      winrt::check_bool(EmptyClipboard());
      for (auto& item : data)
      {
        //The clipboard owns the memory once it is set
        auto memory = ToGlobal(item.Bytes);
        if (!SetClipboardData(item.Format, memory))
        {
          GlobalFree(memory);
          winrt::throw_last_error();
        }
      }
      });
  }

  void ClipboardManager::WithClipboard(const function<void()>& action)
  {
    _window->Invoke([&] {
      auto deadline = steady_clock::now() + _openTimeout;
      while (!OpenClipboard(_window->Handle()))
      {
        if (steady_clock::now() > deadline) throw runtime_error("Another application keeps the clipboard open.");
        this_thread::sleep_for(_openRetryInterval);
      }

      ClipboardCloser closer;
      action();
      });
  }
}
