#include "pch.h"
#include "JsonRpcMessage.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Json;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Rpc;
using namespace std;

namespace SecondShell::Tests
{
  TEST_CLASS(JsonRpcMessageTests)
  {
    using json_value_ptr = value_ptr<json_value>;

    static JsonRpcMessage ParseValid(string_view text)
    {
      auto result = JsonRpcMessage::Parse(text);
      Assert::IsTrue(bool(result), L"the message should parse");
      return *result;
    }

    static int32_t ParseErrorCode(string_view text)
    {
      auto result = JsonRpcMessage::Parse(text);
      Assert::IsFalse(bool(result), L"the message should be rejected");
      return result.error().Code;
    }

    static json_value_ptr ParseJson(string_view text)
    {
      return json_value::from_string(text);
    }

  public:
    TEST_METHOD(RequestIsParsed)
    {
      auto message = ParseValid(R"({"jsonrpc":"2.0","id":7,"method":"tools/list","params":{"cursor":"x"}})");

      Assert::IsTrue(message.Kind() == JsonRpcMessageKind::Request);
      Assert::AreEqual(string("tools/list"), message.Method);
      Assert::AreEqual(7.0, message.Id->as<json_number>().value);
      Assert::AreEqual(string("x"), message.Params->as<json_object>().get_value<string>("cursor"));
    }

    TEST_METHOD(StringIdIsKept)
    {
      auto message = ParseValid(R"({"jsonrpc":"2.0","id":"abc-1","method":"ping"})");

      Assert::AreEqual(string("abc-1"), message.Id->as<json_string>().value);

      //The response echoes the same ID
      auto response = ParseValid(JsonRpcMessage{ .Id = message.Id, .Result = make_value<json_object>() }.ToString());
      Assert::AreEqual(string("abc-1"), response.Id->as<json_string>().value);
    }

    TEST_METHOD(NotificationHasNoId)
    {
      auto message = ParseValid(R"({"jsonrpc":"2.0","method":"notifications/initialized"})");

      Assert::IsTrue(message.Kind() == JsonRpcMessageKind::Notification);
      Assert::IsFalse(bool(message.Params));

      auto text = message.ToString();
      Assert::IsTrue(text.find("\"id\"") == string::npos, L"a notification must not have an ID");
    }

    TEST_METHOD(SuccessResponseIsParsed)
    {
      auto message = ParseValid(R"({"jsonrpc":"2.0","id":3,"result":{"value":1}})");

      Assert::IsTrue(message.Kind() == JsonRpcMessageKind::Response);
      Assert::IsFalse(message.Error.has_value());
      Assert::AreEqual(1.0, message.Result->as<json_object>().get_value<double>("value"));
    }

    TEST_METHOD(ErrorResponseIsParsed)
    {
      auto message = ParseValid(R"({"jsonrpc":"2.0","id":3,"error":{"code":-32601,"message":"Unknown method"}})");

      Assert::IsTrue(message.Kind() == JsonRpcMessageKind::Response);
      Assert::IsTrue(message.Error.has_value());
      Assert::AreEqual(int32_t(JsonRpcErrorCode::MethodNotFound), message.Error->Code);
      Assert::AreEqual(string("Unknown method"), message.Error->Message);
    }

    TEST_METHOD(EmptyResultIsWritten)
    {
      //An empty result is still a result, otherwise the response would be invalid
      auto text = JsonRpcMessage{ .Id = make_value<json_number>(1.0), .Result = make_value<json_object>() }.ToString();
      auto json = ParseJson(text);

      Assert::IsTrue(json->as<json_object>().value.contains("result"));
      Assert::IsFalse(json->as<json_object>().value.contains("error"));
    }

    TEST_METHOD(ErrorResponseHasNoResult)
    {
      auto text = JsonRpcMessage{ .Id = make_value<json_number>(1.0), .Error = JsonRpcError{ JsonRpcErrorCode::InternalError, "Failed" } }.ToString();
      auto json = ParseJson(text);

      Assert::IsTrue(json->as<json_object>().value.contains("error"));
      Assert::IsFalse(json->as<json_object>().value.contains("result"));
    }

    TEST_METHOD(ResponseWithoutIdHasNullId)
    {
      auto text = JsonRpcMessage{ .Error = JsonRpcError{ JsonRpcErrorCode::ParseError, "Not JSON" } }.ToString();
      auto json = ParseJson(text);

      Assert::IsTrue(json->as<json_object>().at("id")->type() == json_type::null);
    }

    TEST_METHOD(RequestRoundtrips)
    {
      JsonRpcMessage request{ .Id = make_value<json_number>(42.0), .Method = "sequence/run", .Params = ParseJson(R"({"steps":[1,2]})") };
      auto parsed = ParseValid(request.ToString());

      Assert::IsTrue(parsed.Kind() == JsonRpcMessageKind::Request);
      Assert::AreEqual(string("sequence/run"), parsed.Method);
      Assert::AreEqual(42.0, parsed.Id->as<json_number>().value);
      Assert::AreEqual(size_t(2), parsed.Params->as<json_object>().at("steps")->as<json_array>().value.size());
    }

    TEST_METHOD(InvalidJsonIsParseError)
    {
      Assert::AreEqual(int32_t(JsonRpcErrorCode::ParseError), ParseErrorCode("{not json"));
    }

    TEST_METHOD(InvalidMessagesAreInvalidRequests)
    {
      auto invalidRequest = int32_t(JsonRpcErrorCode::InvalidRequest);
      Assert::AreEqual(invalidRequest, ParseErrorCode(R"([1,2])"), L"not an object");
      Assert::AreEqual(invalidRequest, ParseErrorCode(R"({"id":1,"method":"ping"})"), L"no version");
      Assert::AreEqual(invalidRequest, ParseErrorCode(R"({"jsonrpc":"1.0","id":1,"method":"ping"})"), L"wrong version");
      Assert::AreEqual(invalidRequest, ParseErrorCode(R"({"jsonrpc":"2.0","id":1,"method":""})"), L"empty method");
      Assert::AreEqual(invalidRequest, ParseErrorCode(R"({"jsonrpc":"2.0","id":{},"method":"ping"})"), L"object ID");
      Assert::AreEqual(invalidRequest, ParseErrorCode(R"({"jsonrpc":"2.0","id":1,"method":"ping","params":5})"), L"number params");
      Assert::AreEqual(invalidRequest, ParseErrorCode(R"({"jsonrpc":"2.0","id":1})"), L"neither request nor response");
    }
  };
}
