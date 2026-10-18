#include "pch.h"
#include "DriverLink.h"
#include "JsonRpcPeer.h"
#include "PipeTransport.h"
#include "TestPipeName.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Networking;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Protocol;
using namespace SecondShell::Rpc;
using namespace SecondShell::Session;
using namespace std;
using namespace std::chrono_literals;

namespace SecondShell::Tests
{
  namespace
  {
    //The driver's side of the pipe, enough to say hello.
    class TestDriver
    {
    public:
      explicit TestDriver(string_view pipeName) :
        _client(pipeName),
        _connectedSubscription(_client.connected({ this, &TestDriver::OnConnected }))
      {
        _client.open();
        Assert::IsTrue(_connected.wait(5s), L"the driver should connect");
      }

      ~TestDriver()
      {
        _connectedSubscription.reset();
      }

      DriverHelloResponse Hello()
      {
        CancellationToken noCancellation;
        auto result = _peer->Request(DriverHelloRequest{ "test" }, 5s, noCancellation);
        if (auto error = get_if<JsonRpcError>(&result)) Assert::Fail(to_wstring(error->Message).c_str());
        return get<DriverHelloResponse>(result);
      }

    private:
      named_pipe_messaging_client _client;
      event_subscription _connectedSubscription;
      Axodox::Threading::manual_reset_event _connected;
      unique_ptr<JsonRpcPeer> _peer;

      void OnConnected(messaging_client* /*client*/, messaging_channel* channel)
      {
        //The first connection only: the client reconnects if the host drops it
        if (_peer) return;

        _peer = make_unique<JsonRpcPeer>(make_shared<PipeTransport>(static_cast<named_pipe_messaging_channel*>(channel)));
        _peer->Start();
        _connected.set();
      }
    };
  }

  //The host's half of the session guard. The test driver runs in this process, so in this session.
  TEST_CLASS(DriverLinkTests)
  {
    string _pipeName = UniquePipeName("Driver");
    dependency_container _container;
    shared_ptr<DriverLink> _link;
    uint32_t _sessionId = 0;

  public:
    DriverLinkTests()
    {
      _link = make_shared<DriverLink>(&_container, _pipeName);
      _link->Start();

      DWORD sessionId = 0;
      ProcessIdToSessionId(GetCurrentProcessId(), &sessionId);
      _sessionId = sessionId;
    }

    TEST_METHOD(DriversAreRefusedWhenNoSessionWaits)
    {
      TestDriver driver{ _pipeName };
      Assert::IsFalse(driver.Hello().IsAccepted);
      Assert::IsFalse(_link->IsConnected());
    }

    TEST_METHOD(DriversInAnotherSessionAreRefused)
    {
      _link->Expect(_sessionId + 1000);

      TestDriver driver{ _pipeName };
      auto response = driver.Hello();
      Assert::IsFalse(response.IsAccepted);
      Assert::IsTrue(response.Reason.find(format("session {}", _sessionId)) != string::npos, to_wstring(response.Reason).c_str());
    }

    TEST_METHOD(TheDriverInTheExpectedSessionIsAcceptedOnce)
    {
      _link->Expect(_sessionId);

      TestDriver driver{ _pipeName };
      Assert::IsTrue(driver.Hello().IsAccepted);
      CancellationToken noCancellation;
      Assert::IsTrue(_link->WaitForConnection(1s, noCancellation));
      Assert::AreEqual(uint32_t(GetCurrentProcessId()), *_link->DriverProcessId());

      TestDriver second{ _pipeName };
      Assert::IsFalse(second.Hello().IsAccepted, L"only one driver");

      _link->Expect(nullopt);
      Assert::IsFalse(_link->IsConnected(), L"dropped when no session is expected");
    }
  };
}
