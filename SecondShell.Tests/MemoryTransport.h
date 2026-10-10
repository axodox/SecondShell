#pragma once
#include "IMessageTransport.h"

namespace SecondShell::Tests
{
  //Two connected in-memory transports. Like a pipe, each delivers its messages on its own reading thread, in order.
  class MemoryTransport : public Rpc::IMessageTransport
  {
  public:
    static std::pair<std::shared_ptr<MemoryTransport>, std::shared_ptr<MemoryTransport>> CreatePair();

    MemoryTransport();
    ~MemoryTransport();

    virtual void Send(std::string_view message) override;
    virtual bool IsOpen() const override;

    //Closes both ends, as when the other process exits.
    void Close();

  private:
    std::weak_ptr<MemoryTransport> _other;
    std::atomic_bool _isOpen = true;
    Axodox::Threading::blocking_collection<std::string> _inbox;
    Axodox::Threading::background_thread _reader;

    void ReadMessages();
    void CloseThisEnd();
  };
}
