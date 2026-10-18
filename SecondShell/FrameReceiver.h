#pragma once
#include "DriverLink.h"
#include "SessionManager.h"
#include "SharedFrames.h"

namespace SecondShell::Rendering
{
  //The driver's shared frames, for the session view: subscribes once the session is ready, follows the driver to new
  //rings (e.g. after a resolution change), and owns their handles.
  class FrameReceiver
  {
    inline static const Axodox::Infrastructure::logger _logger{ "FrameReceiver" };

  public:
    explicit FrameReceiver(Axodox::Infrastructure::dependency_container* container);

    //Changes whenever the frames do: shared, moved to a new ring, or gone.
    uint64_t Generation() const;

    //The adapter the frames are on, which a reader's device must use; empty if there are no frames.
    std::optional<uint64_t> AdapterLuid() const;

    //A reader of the current frames, nullptr if there are none; generation is set to theirs.
    std::unique_ptr<Graphics::SharedFrameReader> CreateReader(ID3D11Device1* device, uint64_t& generation) const;

  private:
    std::shared_ptr<Session::DriverLink> _driver;
    std::shared_ptr<Session::SessionManager> _sessions;
    Axodox::Infrastructure::event_subscription _framesChangedSubscription;
    Axodox::Infrastructure::event_subscription _stateChangedSubscription;

    mutable std::mutex _mutex;
    std::optional<Protocol::SharedFrames> _frames;
    std::vector<winrt::handle> _handles;
    std::atomic<uint64_t> _generation = 0;

    void OnStateChanged(Session::SessionManager* sender, Protocol::SessionState state);
    void OnFramesChanged(Session::DriverLink* sender, const Protocol::SharedFrames& frames);
    void Replace(std::optional<Protocol::SharedFrames> frames);
  };
}
