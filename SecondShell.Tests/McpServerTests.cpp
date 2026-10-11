#include "pch.h"
#include "JsonSnapshot.h"
#include "McpServer.h"
#include "McpTestClient.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Json;
using namespace Axodox::Threading;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Actions;
using namespace SecondShell::Mcp;
using namespace SecondShell::Protocol;
using namespace SecondShell::Rpc;
using namespace std;
using namespace std::chrono_literals;

namespace SecondShell::Tests
{
#pragma region Test tools
  namespace
  {
    struct SumResult
    {
      static json_object_descriptor<SumResult> json_description;

      int32_t Sum = 0;
    };

    struct AddArguments
    {
      static json_object_descriptor<AddArguments> json_description;

      int32_t A = 0;
      int32_t B = 0;
    };

    struct CountArguments
    {
      static json_object_descriptor<CountArguments> json_description;

      uint32_t To = 0;
    };

    struct NoArguments
    {
      static json_object_descriptor<NoArguments> json_description;
    };

    json_object_descriptor<SumResult> SumResult::json_description = describe_json_object<SumResult>({
      { &SumResult::Sum, "sum" }
      });

    json_object_descriptor<AddArguments> AddArguments::json_description = describe_json_object<AddArguments>(
      "Adds two numbers.",
      {
        { &AddArguments::A, "a", { .is_required = true } },
        { &AddArguments::B, "b", { .is_required = true } }
      });

    json_object_descriptor<CountArguments> CountArguments::json_description = describe_json_object<CountArguments>(
      "Counts, reporting each number as progress.",
      {
        { &CountArguments::To, "to", { .is_required = true } }
      });

    json_object_descriptor<NoArguments> NoArguments::json_description = describe_json_object<NoArguments>("Takes no arguments.", {});

    class TestToolSource : public ToolSource
    {
    public:
      manual_reset_event WaitCancelled;

      TestToolSource()
      {
        AddTool("Add", &TestToolSource::Add);
        AddTool("Count", &TestToolSource::Count);
        AddTool("Wait", &TestToolSource::Wait);
        AddTool("Fail", &TestToolSource::Fail);
      }

    private:
      RpcResult<SumResult> Add(const AddArguments& arguments, ToolContext& /*context*/)
      {
        return SumResult{ arguments.A + arguments.B };
      }

      RpcResult<SumResult> Count(const CountArguments& arguments, ToolContext& context)
      {
        for (uint32_t number = 1; number <= arguments.To; number++)
        {
          if (context.ReportProgress) context.ReportProgress(number, arguments.To, to_string(number));
        }
        return SumResult{ int32_t(arguments.To) };
      }

      RpcResult<EmptyResponse> Wait(const NoArguments& /*arguments*/, ToolContext& context)
      {
        if (context.Cancellation.WaitFor(5s)) WaitCancelled.set();
        return EmptyResponse{};
      }

      RpcResult<EmptyResponse> Fail(const NoArguments& /*arguments*/, ToolContext& /*context*/)
      {
        return ProtocolErrors::Make(ErrorCode::UserInterrupted, "The user pressed Stop.");
      }
    };
  }
#pragma endregion

  TEST_CLASS(McpServerTests)
  {
    McpTestClient _client;
    dependency_container _container;
    shared_ptr<TestToolSource> _tools = make_shared<TestToolSource>();

    static string Text(const json_value& toolResult, size_t index = 0)
    {
      return toolResult.as<json_object>().at("content")->as<json_array>().value.at(index)->as<json_object>().at("text")->as<json_string>().value;
    }

    static bool IsError(const json_value& toolResult)
    {
      json_value* isError;
      return toolResult.as<json_object>().try_get_value("isError", isError) && isError->as<json_boolean>().value;
    }

  public:
    McpServerTests()
    {
      _container.add<IMessageTransport>(_client.ServerTransport());
      _container.resolve<registry<ToolSource>>()->add_permanent(_tools);
      _container.resolve<McpServer>()->Start();
    }

    TEST_METHOD(InitializeAnswersInTheClientsRevision)
    {
      auto result = _client.Call("initialize", R"({"protocolVersion": "2025-06-18", "capabilities": {}, "clientInfo": {"name": "test", "version": "1"}})");
      auto& response = result->as<json_object>();
      Assert::AreEqual(string("2025-06-18"), response.at("protocolVersion")->as<json_string>().value);
      Assert::AreEqual(string("SecondShell"), response.at("serverInfo")->as<json_object>().at("name")->as<json_string>().value);
      JsonSnapshot::AssertEqual(R"({"tools": {"listChanged": false}})", *response.at("capabilities"));
      Assert::IsTrue(response.value.contains("instructions"));

      result = _client.Call("initialize", R"({"protocolVersion": "1999-01-01"})");
      Assert::AreEqual(string("2025-11-25"), result->as<json_object>().at("protocolVersion")->as<json_string>().value, L"an unknown revision is answered with the newest");
    }

    TEST_METHOD(PingIsAnswered)
    {
      _client.Send(R"({"jsonrpc": "2.0", "id": "a", "method": "ping"})");
      JsonSnapshot::AssertEqual(R"({"jsonrpc": "2.0", "id": "a", "result": {}})", *_client.Receive());
    }

    TEST_METHOD(ToolsAreListedWithTheirSchemas)
    {
      auto result = _client.Call("tools/list", "{}");
      auto& tools = result->as<json_object>().at("tools")->as<json_array>().value;
      Assert::AreEqual(size_t(4), tools.size());

      JsonSnapshot::AssertEqual(R"({
        "name": "Add",
        "description": "Adds two numbers.",
        "inputSchema": {
          "type": "object",
          "properties": { "a": { "type": "number" }, "b": { "type": "number" } },
          "required": ["a", "b"],
          "additionalProperties": false
        },
        "outputSchema": {
          "type": "object",
          "properties": { "sum": { "type": "number" } }
        }
      })", *tools[0]);
    }

    TEST_METHOD(ToolResultsAreStructuredContent)
    {
      _client.Send(R"({"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": "Add", "arguments": {"a": 1, "b": 2}}})");
      JsonSnapshot::AssertEqual(R"({
        "jsonrpc": "2.0",
        "id": 1,
        "result": {
          "content": [{ "type": "text", "text": "{\"sum\":3}" }],
          "structuredContent": { "sum": 3 }
        }
      })", *_client.Receive());
    }

    TEST_METHOD(InvalidArgumentsAreToolErrors)
    {
      auto result = _client.CallTool("Add", R"({"a": 1, "c": 2})");
      Assert::IsTrue(IsError(*result));
      Assert::AreEqual(string("Invalid arguments: unknown property 'c'; the properties are: a, b."), Text(*result));

      result = _client.CallTool("Add", R"({"a": 1})");
      Assert::IsTrue(IsError(*result));
      Assert::AreEqual(string("Invalid arguments: 'b' is required."), Text(*result));
    }

    TEST_METHOD(ToolErrorsNameTheirCode)
    {
      auto result = _client.CallTool("Fail", "{}");
      Assert::IsTrue(IsError(*result));
      Assert::AreEqual(string("UserInterrupted: The user pressed Stop."), Text(*result));
    }

    TEST_METHOD(UnknownToolsAreProtocolErrors)
    {
      _client.Send(R"({"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": "Subtract", "arguments": {}}})");
      JsonSnapshot::AssertEqual(R"({"jsonrpc": "2.0", "id": 1, "error": {"code": -32602, "message": "Unknown tool Subtract."}})", *_client.Receive());
    }

    TEST_METHOD(CancelledCallsAreNotAnswered)
    {
      _client.Send(R"({"jsonrpc": "2.0", "id": 7, "method": "tools/call", "params": {"name": "Wait"}})");
      Assert::IsTrue(_client.IsSilent(100ms), L"the tool should be waiting");

      _client.Send(R"({"jsonrpc": "2.0", "method": "notifications/cancelled", "params": {"requestId": 7, "reason": "The user changed their mind."}})");
      Assert::IsTrue(_tools->WaitCancelled.wait(5s), L"the tool should see the cancellation");

      _client.Send(R"({"jsonrpc": "2.0", "id": 8, "method": "ping"})");
      JsonSnapshot::AssertEqual(R"({"jsonrpc": "2.0", "id": 8, "result": {}})", *_client.Receive());
    }

    TEST_METHOD(ProgressIsReportedWhenAskedFor)
    {
      _client.Send(R"({"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": "Count", "arguments": {"to": 2}, "_meta": {"progressToken": "p"}}})");
      JsonSnapshot::AssertEqual(R"({"jsonrpc": "2.0", "method": "notifications/progress", "params": {"progressToken": "p", "progress": 1, "total": 2, "message": "1"}})", *_client.Receive());
      JsonSnapshot::AssertEqual(R"({"jsonrpc": "2.0", "method": "notifications/progress", "params": {"progressToken": "p", "progress": 2, "total": 2, "message": "2"}})", *_client.Receive());
      Assert::IsTrue(_client.Receive()->as<json_object>().value.contains("result"));

      auto result = _client.CallTool("Count", R"({"to": 2})");
      JsonSnapshot::AssertEqual(R"({"sum": 2})", *result->as<json_object>().at("structuredContent"));
    }
  };
}
