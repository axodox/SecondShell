#pragma once
#include "CancellationToken.h"
#include "DriverProtocol.h"
#include "JsonRpcError.h"

namespace SecondShell::Session
{
  //The host's side of the driver connection: what the sequencing uses. Tests use a fake.
  class IDriverLink
  {
    Axodox::Infrastructure::event_owner _events;

  public:
    virtual ~IDriverLink() = default;

    virtual bool IsConnected() const = 0;
    virtual std::optional<uint32_t> DriverProcessId() const = 0;

    //Fails with SessionNotActive if no driver is connected.
    virtual Rpc::RpcResult<Protocol::ExecuteActionResponse> Execute(const Protocol::ExecuteActionRequest& request, std::chrono::milliseconds timeout, const Rpc::CancellationToken& cancellation) = 0;
    virtual Rpc::RpcResult<Protocol::ResetInputResponse> ResetInput() = 0;

    //An async action finished; raised on the connection's thread.
    Axodox::Infrastructure::event_publisher<IDriverLink*, const Protocol::ActionCompletedNotification&> ActionCompleted;

  protected:
    IDriverLink();

    void OnActionCompleted(const Protocol::ActionCompletedNotification& notification);
  };
}
