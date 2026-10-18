#pragma once
#include "DriverProtocol.h"
#include "HostConnection.h"
#include "ScreenCapturer.h"
#include "SharedFrames.h"

namespace SecondShell::Capture
{
  //Shares the captured frames with the host for its session view (frames/subscribe): writes each frame into a ring of
  //shared textures, and makes a new ring when the frames change size (frames/changed). Nothing is written before the
  //host subscribes.
  class FramePublisher
  {
    inline static const Axodox::Infrastructure::logger _logger{ "FramePublisher" };

  public:
    //Adds its handler to the host connection, between HostConnection::Open and Hello.
    explicit FramePublisher(Axodox::Infrastructure::dependency_container* container);

  private:
    std::shared_ptr<Driver::HostConnection> _connection;
    std::shared_ptr<ScreenCapturer> _capturer;
    Axodox::Infrastructure::event_subscription _frameArrivedSubscription;

    std::mutex _mutex;
    winrt::handle _host;
    std::unique_ptr<Graphics::SharedFrameWriter> _writer;

    Rpc::RpcResult<Protocol::SubscribeFramesResponse> Subscribe(const Protocol::SubscribeFramesRequest& request, const Rpc::CancellationToken& cancellation);
    void OnFrameArrived(ScreenCapturer* sender, const CapturedFrame& frame);

    //With the lock held
    Protocol::SharedFrames Share() const;
  };
}
