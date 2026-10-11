#include "pch.h"
#include "StdioTransport.h"

using namespace std;

namespace SecondShell::Mcp
{
  StdioTransport::StdioTransport()
  {
    //Keep the original stdout for the messages, and point the Win32 handle and the C runtime's stdout at stderr
    auto process = GetCurrentProcess();
    winrt::check_bool(DuplicateHandle(process, GetStdHandle(STD_OUTPUT_HANDLE), process, _output.put(), 0, FALSE, DUPLICATE_SAME_ACCESS));
    winrt::check_bool(SetStdHandle(STD_OUTPUT_HANDLE, GetStdHandle(STD_ERROR_HANDLE)));
    if (_dup2(_fileno(stderr), _fileno(stdout)) != 0) throw runtime_error("Could not redirect stdout.");

    //Messages are UTF-8 and end with \n; text mode would also end the input at a Ctrl+Z byte
    _setmode(_fileno(stdin), _O_BINARY);
  }

  void StdioTransport::Send(string_view message)
  {
    if (!_isOpen) throw runtime_error("The client closed the connection.");

    string line{ message };
    line += '\n';

    lock_guard lock{ _outputMutex };
    auto data = line.data();
    auto remaining = DWORD(line.size());
    while (remaining > 0)
    {
      DWORD written = 0;
      winrt::check_bool(WriteFile(_output.get(), data, remaining, &written, nullptr));
      data += written;
      remaining -= written;
    }
  }

  bool StdioTransport::IsOpen() const
  {
    return _isOpen;
  }

  void StdioTransport::Run()
  {
    string line;
    while (getline(cin, line))
    {
      if (line.ends_with('\r')) line.pop_back();
      if (!line.empty()) OnMessageReceived(line);
    }

    _isOpen = false;
    OnClosed();
  }
}
