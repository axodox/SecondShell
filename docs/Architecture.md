# SecondShell – Architecture plan

This document turns [Design.md](Design.md) into a code structure: the projects, their major classes and the public API of each class. It is a plan, not a specification of every member: private members, helpers and small value types are mostly omitted, and signatures may change during implementation as long as each class keeps its single purpose.

The structure follows the patterns of Project Asgard (`H:\work\asgard`, the code using Axodox.Common, e.g. `Holomaps.Mcp.Shared` and `Holomaps.Streamer`). Asgard's MCP stack is used as inspiration, not as a dependency.

## 1. Principles

### Readable code

These apply to every line of the implementation, for human and AI readers alike:

- **Small classes and methods, each solving one problem at one level of abstraction.** A method either coordinates steps or does one of them, not both. Responsibilities are not mixed.
- **Lean, not layered.** No abstraction without a present need: no interface with a single implementation (unless a test needs it), no wrapper that only forwards calls, no generic framework for a problem that occurs once. Two or three levels between an entry point and the work is normal; seven is a design problem.
- **Readable without comments.** Names say what code does. Comments explain only what the code can't: external constraints, ordering or locking requirements, workarounds, links to documentation. Longer functions may be split into sections with one-line section comments (`//Connect to the session`).

### Style

- Asgard's coding conventions (Design.md, Implementation): PascalCase for application types, methods, public members and events; camelCase for arguments and locals; `_underscoreCamelCase` for private members and private static constants; snake_case for library-like code. Two-space indentation, braces on their own line.
- Namespaces per domain under the root namespace `SecondShell` (section 2), not per folder.
- Each project has a flat folder of `.h` / `.cpp` pairs, one type per pair, and the precompiled header `pch.h`. Larger files are split into `#pragma region`s.
- `.cpp` files start with `#include "pch.h"` and their own header, then a sorted block of `using namespace` lines, then the code inside its `namespace` block, with static constants at the top.
- Each class that logs declares `inline static const Axodox::Infrastructure::logger _logger{ "ClassName" };` as its first member.

### Dependency injection

Each process has one `Axodox::Infrastructure::dependency_container`, created by its entry point (the *composition root*). There is no global container: the container is passed around explicitly.

- **Services are singletons** that take the container in an explicit constructor and resolve their dependencies in the member initializer list, together with their event subscriptions. The order of the initializer list is deliberate (dependencies first, then the subscriptions to them).
- **The container pointer is not stored.** A service that needs the container later (e.g. in asynchronous code) keeps a `dependency_container_ref` from `container->get_ref()` and calls `try_lock()` when it needs it.
- **The composition root registers** interface-to-implementation mappings (`add<IInterface, Implementation>()`), instances that need explicit construction (`add<T>(instance)`), and the contributions to registries. Everything else is created by the container on first `resolve<T>()`. There are no separate registration classes.
- **Interfaces only where there is a real alternative**: input injectors (`InputInjector` implementation, Win32 fallback), message transports (stdio, pipe), the driver link (a fake in tests).

```cpp
//SequenceRunner.h
namespace SecondShell::Agent
{
  class SequenceRunner
  {
    inline static const Axodox::Infrastructure::logger _logger{ "SequenceRunner" };

  public:
    explicit SequenceRunner(Axodox::Infrastructure::dependency_container* container);

    RunSequenceResponse Run(const RunSequenceRequest& request, const ProgressCallback& progress, const CancellationToken& cancellation);

  private:
    std::shared_ptr<IDriverLink> _driverLink;
    std::shared_ptr<AgentControl> _agentControl;
    Axodox::Infrastructure::event_subscription _modeChangedSubscription;
    std::shared_ptr<CaptureStore> _captureStore;
    std::shared_ptr<ActionLog> _actionLog;

    void OnModeChanged(AgentControl* sender, ControlMode mode);
  };
}

//SequenceRunner.cpp
SequenceRunner::SequenceRunner(dependency_container* container) :
  _driverLink(container->resolve<IDriverLink>()),
  _agentControl(container->resolve<AgentControl>()),
  _modeChangedSubscription(_agentControl->ModeChanged({ this, &SequenceRunner::OnModeChanged })),
  _captureStore(container->resolve<CaptureStore>()),
  _actionLog(container->resolve<ActionLog>())
{ }
```

### Registries

Extension points use Axodox.Common's `registry<T>`: consumers resolve `registry<T>` from the container and iterate it (or subscribe to `item_added` / `item_removed`); contributors add to it in the composition root with `add_permanent`, or keep the RAII `registration` returned by `add` when the contribution is temporary.

| Registry | Contributions | Consumer |
|---|---|---|
| `registry<ToolSource>` | `SessionToolSource` | `McpServer` (tools/list, tools/call) |
| `registry<AiClientIntegration>` | one per supported AI client | `IntegrationManager` |
| `registry<OverlayElement>` | cursor, touch, keystrokes, gamepad, capture flash, activity | `OverlayRenderer` |

### Events

- A class with events declares `Axodox::Infrastructure::event_owner _events;` before its events, and its events as public `event_publisher` members named in PascalCase (`StateChanged`, `FrameArrived`). They are raised with `_events.raise(StateChanged, this, state)`.
- The first event argument is the sender pointer; the second, if any, is a value, a `const T&`, or a `XxxEventArgs` struct.
- Subscribers bind `{ this, &Class::OnXxx }` and keep the `event_subscription` as `_xxxSubscription`, usually initialized in the constructor's initializer list. `no_revoke` only when the subscriber owns the sender.
- No lock is held while raising events or subscribing.

### JSON types

All data that is serialized (messages, actions, settings, MCP types) uses Axodox.Common's descriptor-based JSON (`JsonSchema.h`): plain structs with a `static json_object_descriptor<T> json_description`, defined in the `.cpp` with `describe_json_object`. The descriptor gives each type its name, description and properties; it serializes, validates, and generates the JSON Schema. Polymorphic types derive from a described base; the base's `type_discriminator` names the property that selects the derived type.

```cpp
//MouseActions.h
namespace SecondShell::Actions
{
  struct MouseClickAction : public Action
  {
    static Axodox::Json::json_object_descriptor<MouseClickAction> json_description;

    MouseButton Button = MouseButton::Left;
    std::optional<int32_t> X;
    std::optional<int32_t> Y;
    std::optional<uint32_t> Duration;
  };
}

//MouseActions.cpp
json_object_descriptor<MouseClickAction> MouseClickAction::json_description = describe_json_object<MouseClickAction, Action>(
  { .name = "Mouse.Click", .description = "Clicks a mouse button, optionally after moving to (x, y)." },
  {
    { &MouseClickAction::Button, "button" },
    { &MouseClickAction::X, "x", { .schema = { .description = "Session pixels." } } },
    { &MouseClickAction::Y, "y", { .schema = { .description = "Session pixels." } } },
    { &MouseClickAction::Duration, "duration", { .schema = { .description = "Press duration in milliseconds." } } }
  });
```

The exact `describe_json_object` overloads and option fields are those of the Axodox.Common version in use; the example shows the intent.

### Threads

- Each connection reads on its own background thread and raises events there.
- Code that touches the UI marshals with a `fire_and_forget` handler that takes a strong reference (`get_strong()`) and then `co_await`s the UI thread's `DispatcherQueue`.
- Asynchronous work in services is protected with an Axodox `lifetime_source` (declared last, so its destructor waits for outstanding work), or with `dependency_container_ref::try_lock()`.
- The host renders the session view on a dedicated render thread.
- Only one `Run-Sequence` runs at a time; it runs on the thread of the request that started it.

### Errors and logging

- Boundaries (RPC handlers, event handlers, thread entry points) catch `std::exception`, log it with `_logger`, and turn it into an error response. Exceptions do not cross process boundaries or escape threads.
- Parsing and validation return `Axodox::Infrastructure::expected<>`; call `.check()` where failure is a programming error.
- RPC failures are `JsonRpcError`s with SecondShell's error codes (Design.md, Errors and timeouts).
- `winrt::check_hresult` and friends turn error-code APIs into exceptions.

## 2. Solution layout

| Project | Output | Runs in | Depends on |
|---|---|---|---|
| `SecondShell.Common` | `SecondShell.Common.dll` | all processes | Axodox.Common |
| `SecondShell` | `SecondShell.exe` (WinUI 3) | user's session | Common, Windows App SDK |
| `SecondShell.Driver` | `SecondShell.Driver.exe` | child session | Common |
| `SecondShell.Mcp` | `SecondShell.Mcp.exe` (console) | user's session | Common |
| `SecondShell.Tests` | test DLL | – | Common, plus the testable sources of the other projects |

```
AI client ─stdio─► SecondShell.Mcp ─pipe (JSON-RPC)─► SecondShell (host) ─RDP─► child session
                                                            ▲                        │
                                                            ├─ pipe (JSON-RPC) ── SecondShell.Driver
                                                            └─ shared GPU textures ─┘
```

Namespaces, by domain:

| Namespace | Contents | Project |
|---|---|---|
| `SecondShell::Rpc` | JSON-RPC messages, peer, transports | Common (+ stdio transport in Mcp) |
| `SecondShell::Actions` | Action and result types | Common |
| `SecondShell::Protocol` | Requests, responses and notifications between the processes | Common |
| `SecondShell::Environment` | Paths, pipe names, build differences | Common |
| `SecondShell::Graphics` | Shared frame textures | Common |
| `SecondShell::Driver` | Driver startup, action handlers | Driver |
| `SecondShell::Input` | Input injectors and input state | Driver |
| `SecondShell::Capture` | Screen capture, encoding, OCR, waits | Driver |
| `SecondShell::Desktop` | Windows, processes, clipboard | Driver |
| `SecondShell::Mcp` | MCP server, tool sources, host client | Mcp |
| `SecondShell::Setup` | Setup checks, repair, uninstall | Host |
| `SecondShell::Session` | RDP, child session, credentials, lifecycle | Host |
| `SecondShell::Agent` | Sequences, agent control, captures, log | Host |
| `SecondShell::Rendering` | Session view and overlay | Host |
| `SecondShell::Integrations` | AI client integrations | Host |
| `winrt::SecondShell::implementation` | XAML views, view models, converters | Host |

## 3. SecondShell.Common

### 3.1 Transports (`SecondShell::Rpc`)

The processes communicate with JSON-RPC 2.0 messages. A transport moves message text and nothing else.

```cpp
namespace SecondShell::Rpc
{
  class IMessageTransport
  {
    Axodox::Infrastructure::event_owner _events;

  public:
    virtual void Send(std::string_view message) = 0;  //thread-safe
    virtual bool IsOpen() const = 0;
    virtual ~IMessageTransport() = default;

    Axodox::Infrastructure::event_publisher<IMessageTransport*, std::string_view> MessageReceived;
    Axodox::Infrastructure::event_publisher<IMessageTransport*> Closed;

  protected:
    IMessageTransport();

    //For implementations: raise the events, which only the owner of _events may do.
    void OnMessageReceived(std::string_view message);
    void OnClosed();
  };

  //One JSON-RPC message per message of an Axodox named-pipe messaging channel.
  class PipeTransport : public IMessageTransport
  {
  public:
    explicit PipeTransport(Axodox::Networking::named_pipe_messaging_channel* channel);

    virtual void Send(std::string_view message) override;
    virtual bool IsOpen() const override;

    uint32_t PeerSessionId() const;
    uint32_t PeerProcessId() const;
  };
}
```

A `PipeTransport` is created in the connection's connected event (so no message arrives before it subscribes) and dropped when `Closed` is raised, since the channel belongs to the messaging server or client. Tests use an in-memory transport pair (`SecondShell.Tests\MemoryTransport`), which delivers on its own thread like a pipe; it is not part of the shipped library.

The pipes use Axodox.Common's messaging layer (`messaging_channel` / `messaging_server` / `messaging_client`, which already has TCP implementations) with a named pipe implementation added to Axodox.Common (section 9). Named pipes rather than loopback TCP, because:

- **Access control**: the pipe's access list allows only the current user, and `PIPE_REJECT_REMOTE_CLIENTS` refuses remote clients. A loopback TCP port is open to every local user and process.
- **Session guard**: `GetNamedPipeClientSessionId` tells the host which session the driver runs in. TCP has no equivalent.
- **No ports**: no port conflicts, no firewall prompts. Pipe names are derived from the user's SID (`PipeNames`, section 3.5).

### 3.2 JSON-RPC (`SecondShell::Rpc`)

One JSON-RPC implementation serves all three links: MCP over stdio, MCP server ↔ host and host ↔ driver over pipes. Typed handlers are keyed by their request type's descriptor name, so a method's name, argument type and result type are declared in one place.

```cpp
namespace SecondShell::Rpc
{
  struct JsonRpcError
  {
    static Axodox::Json::json_object_descriptor<JsonRpcError> json_description;

    int32_t Code = 0;  //JsonRpcErrorCode (JSON-RPC's codes, ConnectionClosed, Timeout, RequestCancelled) or an application code
    std::string Message;
    Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> Data;
  };

  //The JSON-RPC 2.0 envelope: request, notification or response, with params and result as raw JSON.
  //Written and parsed by hand, because its fields depend on the kind of message (see below).
  struct JsonRpcMessage
  {
    Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> Id;
    std::string Method;
    Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> Params;
    Axodox::Infrastructure::value_ptr<Axodox::Json::json_value> Result;
    std::optional<JsonRpcError> Error;

    JsonRpcMessageKind Kind() const;
    std::string ToString() const;
    static Axodox::Infrastructure::expected<JsonRpcMessage, JsonRpcError> Parse(std::string_view text);
  };

  template<typename TResponse>
  using RpcResult = std::variant<TResponse, JsonRpcError>;

  //Both ends of a JSON-RPC connection: answers incoming requests with registered handlers,
  //and sends requests and notifications of its own. Matches responses to requests by ID.
  class JsonRpcPeer
  {
    inline static const Axodox::Infrastructure::logger _logger{ "JsonRpcPeer" };

  public:
    explicit JsonRpcPeer(std::shared_ptr<IMessageTransport> transport);

    template<typename TOwner, typename TRequest, typename TResponse>
    void AddHandler(TOwner* owner, RpcResult<TResponse>(TOwner::*handler)(const TRequest& request, const CancellationToken& cancellation));

    template<typename TOwner, typename TNotification>
    void AddNotificationHandler(TOwner* owner, void(TOwner::*handler)(const TNotification& notification));

    template<typename TRequest, typename TResponse = typename TRequest::ResponseType>
    RpcResult<TResponse> Request(const TRequest& request, std::chrono::milliseconds timeout, const CancellationToken& cancellation);

    template<typename TNotification>
    void Notify(const TNotification& notification);

    void Start();
    IMessageTransport* Transport() const;
  };
}
```

- A method is declared by its request type: the descriptor name is the method name, and the type names its response with `using ResponseType = ...;`, so `Request` deduces it.
- Params are validated against the request type's schema before the handler runs; failures become `InvalidParams` errors naming the property. Exceptions in handlers are logged and become `InternalError` responses.
- Threads: each incoming request runs on its own thread (guarded by the peer's `lifetime_source`), so a long request doesn't hold up others. Notifications and responses are handled on the transport's thread, in order; a notification handler therefore must not wait for a response of the same peer.
- Cancellation: MCP's `notifications/cancelled` (`requestId`, `reason`) is used on every link. An incoming one cancels the handler's `CancellationToken`, and the cancelled request gets no response, as MCP specifies. A request whose timeout expires or whose token is cancelled sends it to the other side and returns `Timeout` or `RequestCancelled`.
- A closed transport fails the pending requests with `ConnectionClosed` and cancels the running handlers.
- Why the envelope is written by hand: a request has an ID and a notification does not; a response always has an ID (null if the request's ID could not be read) and exactly one of `result` and `error`, and an empty result must still be written. One descriptor cannot express that. Params and results stay descriptor-based.

`CancellationToken` (`Cancel`, `IsCancelled`, `WaitFor`, a `Cancelled` event) is used by every cancellable operation.

### 3.3 Actions (`SecondShell::Actions`)

Every action of `Run-Sequence` (Design.md, Input simulation) is one described type. The same type serves the MCP server (schema and validation), the host (logging, visualization, sequencing) and the driver (execution).

```cpp
namespace SecondShell::Actions
{
  //Base of all actions; its type discriminator is "action", and each derived type's descriptor name is the action name (e.g. "Mouse.Click").
  struct Action
  {
    static Axodox::Json::json_object_descriptor<Action> json_description;

    std::optional<bool> Async;
    std::optional<uint32_t> Timeout;  //milliseconds, for actions that wait

    virtual ActionKind Kind() const = 0;  //Control, Mouse, Keyboard, Touch, Pen, Gamepad, Capture, Window, Process, Clipboard
    virtual ActionType Type() const = 0;  //named enum, one value per action
    virtual ~Action() = default;
  };

  //Results: a base with status and error, and derived results for actions that return data.
  struct ActionResult
  {
    static Axodox::Json::json_object_descriptor<ActionResult> json_description;

    ActionStatus Status = ActionStatus::Succeeded;  //Succeeded, Failed, Cancelled
    std::optional<ErrorCode> Error;
    std::optional<std::string> Message;

    virtual ~ActionResult() = default;
  };
}
```

- One header per group, each with all actions of its group: `ControlActions.h`, `MouseActions.h`, `KeyboardActions.h`, `TouchActions.h`, `CaptureActions.h`, `WindowActions.h`, `ProcessActions.h`, `ClipboardActions.h`. Pen actions are not designed yet (Design.md has none), and gamepad actions are deferred with gamepad support. Each action derives from `TypedAction<kind, type, TBase = Action>`, which implements `Kind()` and `Type()`; the capture actions share the described base `CaptureAction` (tag, output, path, format, resolution).
- `Action` also has `IsAsync()` and `WaitTimeout()` (virtual, overridden by the actions with `async` / `timeout`), so the sequence runner needs no per-type code. `Action` and `ActionResult` are not abstract, because Axodox descriptors instantiate their types.
- **All action and result descriptors are defined in one file, `Actions.cpp`**: a derived descriptor copies its base's properties and registers with the base while it is constructed, which needs the base constructed first, guaranteed only within one translation unit.
- **`ActionCatalog`** lists every action type. It parses an action by its `"action"` name (case-insensitive) with that type's own descriptor, rejects unknown properties (typos), and builds the JSON schema the AI sees: `anyOf` the actions, each with `"action"` as a constant, its required properties and `additionalProperties: false`. A `json_type_metadata` specialization makes every `value_ptr<Action>` property (e.g. `RunSequenceRequest::Steps`) use this schema and validation. The plain descriptors can't do this: they validate and describe only the static type's properties.
- Enum values are PascalCase identifiers in the schema and parsed case-insensitively (`"inline"` works). Keys use `KeyConverter` / `KeySchema`: a `Keys` name, an alias, or a virtual key code number. `WindowId` is a hex string type.
- Not done yet: numeric properties are described as `"number"` (Axodox has no integer schema), and the process info has CPU and memory only (no GPU, disk, network).
- Results (type discriminator `"type"`): `ActionResult` (status, error, message) and `CaptureResult` (tag, `CapturedImage`: size, scale, format, base64 data, path), `ProcessResult`, `ProcessListResult`, `WindowListResult`, `ClipboardResult`. What was held when a sequence stopped is an `InputState` in `RunSequenceResponse::ReleasedInput` and `ResetInputResponse`.
- Small shared types: `MouseButton`, `GamepadButton`, `GamepadAxis`, `KeyModifier` (named enums), `KeyNames` (the `System.Windows.Forms.Keys` names and aliases mapped to virtual-key codes), `WindowId` (HWND as a hex string), `ClipboardContent` (format and data).

### 3.4 Protocol (`SecondShell::Protocol`)

The requests, responses and notifications between the processes, as described types. Each request's descriptor name is its JSON-RPC method name.

| Link | Method (request → response) | Purpose |
|---|---|---|
| Driver → host | `driver/hello` (`DriverHelloRequest` → `DriverHelloResponse`) | Session guard: the host accepts or rejects the driver |
| Host → driver | `actions/execute` (`ExecuteActionRequest` → `ExecuteActionResponse`) | One action; the response carries its `ActionResult` |
| Host → driver | `input/reset` (`ResetInputRequest` → `ResetInputResponse`) | Release everything (Control.Reset, Stop button, failures) |
| Host → driver | `frames/subscribe` (`SubscribeFramesRequest` → `SubscribeFramesResponse`) | Host's process ID and adapter; the driver returns the shared frame textures (section 3.6) |
| Host → driver | `clipboard/sync` (`SetClipboardSyncRequest` → `SetClipboardSyncResponse`) | Start or stop clipboard monitoring for interactive mode |
| Host → driver | `input/forward` (notification `ForwardUserInputNotification`) | The user's input in interactive mode |
| Driver → host | `actions/completed` (notification) | An asynchronous action finished |
| Driver → host | `clipboard/changed` (notification), and back via `clipboard/set` | Clipboard sync in interactive mode |
| MCP → host | `session/initialize`, `session/check`, `session/shutdown`, `sequence/run`, `display/set`, `display/get`, `capture/configure` | One method per MCP tool |
| Host → MCP | `sequence/progress` (notification) | Progress of a running sequence, forwarded as MCP progress notifications |

`notifications/cancelled` (as in MCP: `requestId`, `reason`) cancels a running request on any link.

Implemented in `DriverProtocol.h` and `HostProtocol.h` (plus `EmptyResponse`, `ProtocolErrors`: `ErrorCode` <-> JSON-RPC code 1000 + value). Each request declares `using ResponseType`. `input/forward` carries the user's input as input actions (`Mouse.Move`, `Keyboard.Press`, ...), so interactive mode needs no event model of its own. `actions/execute` has an `actionId`; async actions answer when started and report through `actions/completed`. `frames/subscribe` returns the texture handles, size and the adapter LUID. The host request types are the arguments of the seven MCP tools; `RunSequenceRequest` extends the tool's `RunSequenceArguments` with the progress token, which the AI does not set.

### 3.5 Environment (`SecondShell::Environment`)

```cpp
namespace SecondShell::Environment
{
  //Everything that differs between the portable and the MSIX build (Design.md, Build differences in code).
  class AppEnvironment
  {
  public:
    AppEnvironment();

    bool IsPackaged() const;
    std::filesystem::path HostExecutable() const;
    std::filesystem::path DriverExecutable() const;
    std::filesystem::path McpExecutable() const;  //alias path when packaged
    std::filesystem::path DataFolder() const;     //real path, also when redirected by MSIX
    std::filesystem::path CaptureFolder(std::string_view sessionName) const;
    std::filesystem::path LogFolder() const;
  };

  //Pipe names and other names shared by the processes.
  struct PipeNames
  {
    static std::string Driver();
    static std::string Mcp();
  };
}
```

### 3.6 Shared frames (`SecondShell::Graphics`)

The driver's captured frames reach the host's swap chain without leaving the GPU: a small ring of shared textures, each guarded by a keyed mutex (`D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX`), as Asgard does with its `GraphicsMutex` / `GraphicsLock`.

```cpp
namespace SecondShell::Graphics
{
  //Writer side (driver): copies each new frame into the next free texture of the ring, under its keyed mutex.
  class SharedFrameWriter
  {
  public:
    SharedFrameWriter(const Axodox::Graphics::GraphicsDevice& device, Size size, uint32_t textureCount);

    std::vector<SharedTextureHandle> ShareWith(uint32_t processId);  //NT handles duplicated into the reader process
    void Write(ID3D11Texture2D* frame);
  };

  //Reader side (host): opens the shared textures and hands out the newest complete frame, locked while in use.
  class SharedFrameReader
  {
  public:
    SharedFrameReader(const Axodox::Graphics::GraphicsDevice& device, std::span<const SharedTextureHandle> handles);

    SharedFrameLock LockLatest(std::chrono::milliseconds timeout);  //RAII: releases the keyed mutex when destroyed
  };
}
```

Two points to verify:

- **Handles, not names, across sessions.** Named shared resources are created in the creating session's namespace, so a name created in the child session isn't visible in the user's session, and `Global\` names need a privilege normal users lack. The driver therefore duplicates the NT handles into the host process (`DuplicateHandle`, same user), and the host opens them with `OpenSharedResource1`.
- **Same adapter.** Both processes must use the same GPU. The driver reports the adapter LUID its frames come from, and the host creates its device on that adapter.

## 4. SecondShell.Driver

Runs in the child session without UI. Executes actions and publishes frames.

### 4.1 Startup and dispatch (`SecondShell::Driver`)

```cpp
//Composition root: per-monitor DPI awareness, container, connection; runs until the host disconnects.
class DriverApplication
{
public:
  int Run();
};

//Connects to the host's driver pipe and performs the session guard (driver/hello).
//Nothing else in the driver runs before Connect returns true.
class HostConnection
{
public:
  explicit HostConnection(Axodox::Infrastructure::dependency_container* container);

  bool Connect(std::chrono::milliseconds timeout);  //false: no host, or rejected -> exit immediately
  Rpc::JsonRpcPeer& Peer();
};

//Registers the driver's RPC handlers; routes each action to the handler for its kind; runs async actions in the background.
class ActionExecutor
{
public:
  explicit ActionExecutor(Axodox::Infrastructure::dependency_container* container);

  void Start();

private:
  Rpc::RpcResult<Protocol::ExecuteActionResponse> OnExecuteAction(const Protocol::ExecuteActionRequest& request, const CancellationToken& cancellation);
  Rpc::RpcResult<Protocol::ResetInputResponse> OnResetInput(const Protocol::ResetInputRequest& request, const CancellationToken& cancellation);
};
```

Dispatch follows Asgard's pattern: `ActionExecutor` switches on `action.Kind()` and calls the handler of that group; each handler switches on `action.Type()` and `static_cast`s to the concrete action. Handlers: `MouseActionHandler`, `KeyboardActionHandler`, `TouchActionHandler`, `PenActionHandler`, `GamepadActionHandler`, `CaptureActionHandler`, `WindowActionHandler`, `ProcessActionHandler`, `ClipboardActionHandler`. A handler only translates actions into calls on the services below:

```cpp
class MouseActionHandler
{
public:
  explicit MouseActionHandler(Axodox::Infrastructure::dependency_container* container);

  std::unique_ptr<Actions::ActionResult> Execute(const Actions::Action& action, const CancellationToken& cancellation);

private:
  std::unique_ptr<Actions::ActionResult> Move(const Actions::MouseMoveAction& action, const CancellationToken& cancellation);
  std::unique_ptr<Actions::ActionResult> Click(const Actions::MouseClickAction& action, const CancellationToken& cancellation);
  //...one method per action
};
```

### 4.2 Input injection (`SecondShell::Input`)

```cpp
//Owns the WinRT InputInjector; initializes touch, pen and gamepad injection on first use.
class InputInjectorProvider
{
public:
  InputInjectorProvider();

  bool IsAvailable() const;  //TryCreate succeeded
  winrt::Windows::UI::Input::Preview::Injection::InputInjector& Injector();
  void EnsureTouch();
  void EnsurePen();
  void EnsureGamepad();
};

class IMouseInjector
{
public:
  virtual void MoveTo(Point position) = 0;
  virtual void Press(Actions::MouseButton button) = 0;
  virtual void Release(Actions::MouseButton button) = 0;
  virtual void Wheel(float notches, WheelAxis axis) = 0;
  virtual ~IMouseInjector() = default;
};

class IKeyboardInjector
{
public:
  virtual void Press(uint16_t virtualKey) = 0;
  virtual void Release(uint16_t virtualKey) = 0;
  virtual void TypeCharacter(char32_t character) = 0;  //Unicode, layout-independent
  virtual ~IKeyboardInjector() = default;
};

class ITouchInjector
{
public:
  virtual void Update(std::span<const TouchContact> contacts) = 0;  //all active contacts of one frame
  virtual ~ITouchInjector() = default;
};

class IPenInjector
{
public:
  virtual void Update(const PenState& state) = 0;
  virtual ~IPenInjector() = default;
};

class IGamepadInjector
{
public:
  virtual void Update(const GamepadState& state) = 0;  //buttons, sticks, triggers
  virtual ~IGamepadInjector() = default;
};
```

Implementations: `MouseInjector`, `KeyboardInjector`, `TouchInjector`, `PenInjector`, `GamepadInjector`, all using `InputInjectorProvider`. Win32 fallbacks (`Win32MouseInjector`, `Win32KeyboardInjector`, `SyntheticPointerTouchInjector`, `SyntheticPointerPenInjector`) implement the same interfaces; the composition root registers them instead if `InputInjectorProvider::IsAvailable()` is false.

```cpp
//What is held (buttons, keys, touch points, gamepad state), so it can be reported and released.
class InputStateTracker
{
public:
  explicit InputStateTracker(Axodox::Infrastructure::dependency_container* container);

  void OnPressed(InputKey key);
  void OnReleased(InputKey key);
  InputStateSnapshot Snapshot() const;
  void ReleaseAll();  //also returns gamepad sticks and triggers to rest
};

//Interpolates moves and axis changes over a duration (linear for now).
class MotionAnimator
{
public:
  void Animate(std::chrono::milliseconds duration, const std::function<void(float progress)>& step, const CancellationToken& cancellation);
};
```

### 4.3 Capture (`SecondShell::Capture`)

```cpp
//Windows.Graphics.Capture session for the session's desktop; holds the latest frame.
class ScreenCapturer
{
  Axodox::Infrastructure::event_owner _events;

public:
  explicit ScreenCapturer(Axodox::Infrastructure::dependency_container* container);

  void Start();
  void Stop();
  winrt::com_ptr<ID3D11Texture2D> LatestFrame() const;
  Size ScreenSize() const;
  LUID AdapterLuid() const;

  Axodox::Infrastructure::event_publisher<ScreenCapturer*> FrameArrived;
};

//Writes new frames into the shared frame ring for the host (SharedFrameWriter), and answers frames/subscribe.
class FramePublisher
{
public:
  explicit FramePublisher(Axodox::Infrastructure::dependency_container* container);
};

//Crops, scales and encodes a frame to PNG or JPEG (WIC).
class CaptureEncoder
{
public:
  EncodedImage Encode(ID3D11Texture2D* frame, const Rect& region, const std::optional<Size>& maxResolution, ImageFormat format) const;  //bytes, size, scale
};

//Windows.Media.Ocr over a frame region.
class TextRecognizer
{
public:
  std::vector<RecognizedText> Recognize(ID3D11Texture2D* frame, const Rect& region) const;
};

//Capture.WaitForText and Capture.WaitForStable.
class ScreenConditionWaiter
{
public:
  explicit ScreenConditionWaiter(Axodox::Infrastructure::dependency_container* container);

  bool WaitForText(std::string_view text, const Rect& region, const WaitOptions& options, const CancellationToken& cancellation);
  bool WaitForStable(const Rect& region, std::chrono::milliseconds stableFor, const WaitOptions& options, const CancellationToken& cancellation);
};
```

### 4.4 Windows, processes, clipboard (`SecondShell::Desktop`)

```cpp
//Top-level windows of the session.
class WindowManager
{
public:
  std::vector<WindowInfo> Enumerate() const;
  WindowInfo Get(Actions::WindowId window) const;
  bool Focus(Actions::WindowId window);
  void Move(Actions::WindowId window, Point position);
  void Resize(Actions::WindowId window, Size size);
  void Minimize(Actions::WindowId window);
  void Maximize(Actions::WindowId window);
  void Restore(Actions::WindowId window);
  void Close(Actions::WindowId window);  //close request, like the X button
};

//Builds the command line for a shell (PowerShell, Pwsh, Cmd, None).
class ShellCommandBuilder
{
public:
  std::wstring Build(ShellKind shell, std::string_view command, std::span<const std::string> args) const;
};

//Starts, stops and waits for processes; keeps the output of the processes it started.
class ProcessManager
{
public:
  explicit ProcessManager(Axodox::Infrastructure::dependency_container* container);

  ProcessResult Run(const RunOptions& options, const CancellationToken& cancellation);  //synchronous
  uint32_t Start(const RunOptions& options);                                             //asynchronous, returns the PID
  ProcessResult Wait(uint32_t processId, std::chrono::milliseconds timeout, const CancellationToken& cancellation);
  ProcessResult Stop(uint32_t processId, std::chrono::milliseconds timeout);              //graceful
  void Kill(uint32_t processId, bool includeChildren);
  std::vector<ProcessInfo> Enumerate() const;
};

//The session's clipboard: all supported formats, and change notifications.
class ClipboardManager
{
  Axodox::Infrastructure::event_owner _events;

public:
  explicit ClipboardManager(Axodox::Infrastructure::dependency_container* container);

  std::vector<std::string> AvailableFormats() const;
  std::optional<Actions::ClipboardContent> Read(Actions::ClipboardFormat format) const;
  void Write(std::span<const Actions::ClipboardContent> contents);
  void SetMonitoring(bool isEnabled);

  Axodox::Infrastructure::event_publisher<ClipboardManager*> Changed;
};

//Converts between Windows clipboard formats and ClipboardContent (CF_HTML header, DIB <-> PNG, CF_HDROP).
class ClipboardFormatConverter;

//Message-only window on its own thread, for APIs that need a window (clipboard listener).
class MessageWindow
{
  Axodox::Infrastructure::event_owner _events;

public:
  MessageWindow();

  HWND Handle() const;
  void Invoke(const std::function<void()>& action);  //runs on the window's thread

  Axodox::Infrastructure::event_publisher<MessageWindow*, const WindowMessage&> MessageReceived;
};
```

## 5. SecondShell.Mcp (`SecondShell::Mcp`)

A small console program: MCP over stdio on one side, the host's pipe on the other.

**Why a hand-written MCP layer.** There is no official C++ MCP SDK, and the community libraries (fastmcpp, gopher-mcp, TinyMCP, mcp-cpp) are early-stage or beta, target older protocol revisions and bring their own JSON libraries. SecondShell needs only the stdio transport, the server side and tools: a small amount of code on the JSON-RPC peer (section 3.2), shaped after Asgard's `Holomaps.Mcp.Shared` (transport → JSON-RPC → MCP protocol → tool sources). If a mature C++ MCP SDK appears, only `StdioTransport` and `McpServer` would be replaced.

**Schemas from types.** Each tool's argument and result types are described types. Their descriptors give the tool's description, `inputSchema` and `outputSchema`, and parse incoming arguments. The tool definitions the AI client sees, the parsing and the validation can therefore never disagree.

```cpp
//MCP's stdio transport: one message per line. Takes over stdout; everything else written there goes to stderr.
class StdioTransport : public Rpc::IMessageTransport
{
public:
  StdioTransport();

  virtual void Send(std::string_view message) override;
  virtual bool IsOpen() const override;

  void Run();  //reads on the calling thread until the client closes stdin
};

//The MCP methods on the JSON-RPC peer of the IMessageTransport in the container: initialize (version negotiation,
//capabilities), ping, tools/list and tools/call, which go to the tools of registry<ToolSource>.
class McpServer
{
public:
  explicit McpServer(Axodox::Infrastructure::dependency_container* container);

  void Start();  //after the tool sources are registered
};

//What a tool call has besides its arguments.
struct ToolContext
{
  const Rpc::CancellationToken& Cancellation;
  std::function<void(uint32_t completed, uint32_t total, std::string_view message)> ReportProgress;  //empty if not asked for
  std::vector<Axodox::Infrastructure::value_ptr<ContentBlock>> Content;                            //added after the result
};

//A set of tools, registered in registry<ToolSource>. A derived source adds each tool from a member function.
class ToolSource
{
public:
  std::span<const ToolDefinition> Tools() const;

protected:
  template<typename TSource, typename TArguments, typename TResult>
  void AddTool(const char* name, Rpc::RpcResult<TResult>(TSource::*tool)(const TArguments& arguments, ToolContext& context));
};

//The seven SecondShell tools; each forwards to the host through HostClient.
class SessionToolSource : public ToolSource
{
public:
  explicit SessionToolSource(Axodox::Infrastructure::dependency_container* container);
};

//The JSON-RPC connection to the host. Connects when it is needed: on first use, and again after the host restarted.
class HostClient
{
public:
  explicit HostClient(Axodox::Infrastructure::dependency_container* container);
  HostClient(Axodox::Infrastructure::dependency_container* container, std::string pipeName);  //tests

  bool IsRunning();
  std::optional<Rpc::JsonRpcError> Start(const Rpc::CancellationToken& cancellation);  //launches the host unless it runs

  template<typename TRequest, typename TResponse = typename TRequest::ResponseType>
  Rpc::RpcResult<TResponse> Request(const TRequest& request, std::chrono::milliseconds timeout, const Rpc::CancellationToken& cancellation);

  Axodox::Infrastructure::event_publisher<HostClient*, const Protocol::SequenceProgressNotification&> SequenceProgress;
};

//Starts SecondShell.exe through the running Explorer (IShellWindows → desktop view → IShellDispatch2::ShellExecute).
class HostLauncher
{
public:
  explicit HostLauncher(Axodox::Infrastructure::dependency_container* container);

  void Launch();
};
```

- **Composition root**: `main.cpp` creates the `StdioTransport`, registers it as the container's `IMessageTransport`, adds `SessionToolSource` to `registry<ToolSource>`, starts `McpServer` and calls `Run`. Unit tests do the same with an in-memory transport.
- **Messages**: `McpMessages.h` (initialize, ping, tools/list, tools/call, `notifications/progress`) and `McpContent.h` (`ContentBlock` with `TextContent` and `ImageContent`, type discriminator `"type"`). The server answers in the client's protocol revision if it knows it (2025-11-25, 2025-06-18, 2025-03-26, 2024-11-05), otherwise in the newest. Its `instructions` tell the AI what SecondShell is and how to start.
- **Tools**: the name is given to `AddTool`; the description and `inputSchema` come from the argument type (its description moved to the tool, `additionalProperties: false`), `outputSchema` from the result type. Arguments are parsed with `StrictJson` (Common), as actions are: unknown properties are rejected.
- **Results**: a result becomes `structuredContent`, and the same JSON as a text block (MCP's recommendation for clients that don't read structured content), followed by `ToolContext::Content`. Invalid arguments and errors from the host are **tool errors** (`isError`, text `"<ErrorCode>: <message>"` via `ProtocolErrors::ToText`), which the AI sees and can react to; an unknown tool is a JSON-RPC `InvalidParams` error, as MCP specifies. A sequence that stopped at a failed step is a successful call: its result has `failedStep`, `error` and `message`.
- **The seven tools** take the host's request types as arguments (`InitializeSessionRequest`, ...), so their descriptions are written once. `Run-Sequence` takes `RunSequenceArguments`, which `RunSequenceRequest` extends with the progress token. `Set-Display`, `Get-Display` and `Configure-Capture` only forward (`SessionToolSource::Forward<TRequest>`).
- **Starting the host**: only `Initialize-Session` starts it (`HostClient::Start`: launch, then wait up to 30 s for the client to connect). Without a host, `Check-Session` answers with the state `Stopped`, and the other tools fail with `SessionNotActive`. `HostLauncher` goes through Explorer because the host must run outside the AI client's package context (its `RunOnce` entry would be virtualized, Findings.md 2.12) and outside its process tree (clients end their servers' trees, and the host outlives the MCP server).
- **Connections**: `HostClient` owns a `named_pipe_messaging_client`, which it opens on first use and which then connects whenever the host is there (at once, then every second; also after the host restarted). On first use it waits up to a second for that first attempt, so a running host is found before `Start` would launch another one. Each `connected` event makes a new connection (`PipeTransport`, `JsonRpcPeer`), created before the channel opens; requests take the current one, and fail with `SessionNotActive` if it is closed, without connecting themselves. Requests still using a previous connection finish with it.
- **Timeouts**: requests to the host wait for the tool's own timeout (or its default: `InitializeSessionRequest::DefaultTimeout` 60 s, `ShutdownSessionRequest` 30 s, `RunSequenceArguments` 60 s) plus 10 s for the host to answer; tools without a timeout wait 30 s.
- **Progress**: when a `tools/call` has a progress token, `Run-Sequence` sends the host a token of its own; `HostClient` raises `SequenceProgress` for each `sequence/progress`, and the call forwards those with its token as `notifications/progress`.
- **Captures**: inline captures in a `Run-Sequence` result move to image content (with a caption naming the tag, size and scale), and the structured result keeps everything but the data.
- **Known gaps**: unknown properties are rejected only at the top level of tool arguments and actions, not in nested objects (e.g. `display`). Logs go to stderr only when it is a console (Axodox's logger writes with `WriteConsole`), so an AI client sees none; a log file follows with the host. On the host side, `PipeTransport` must not outlive its channel: the host's per-connection objects must be dropped in the server's disconnect event (stage 5).

## 6. SecondShell (host)

The WinUI 3 application.

### 6.1 Application

- The WinUI `App` class is the composition root: it parses the command line (`CommandLineRouter`: normal window, or `--setup`, `--uninstall`, `--integrate`, `--unintegrate`), creates the container, registers the interface mappings and registry contributions, and starts the services.
- XAML runtime classes are created by XAML, not by the container. View models therefore receive a `ViewModelContext` (container and UI `DispatcherQueue`) in their constructor and resolve their services from it; converters use a thread-local reference to the container, as in Asgard.

```cpp
//Decides what the process does.
class CommandLineRouter
{
public:
  static CommandLineAction Parse(std::wstring_view commandLine);
};
```

### 6.2 Setup (`SecondShell::Setup`)

```cpp
//Checks on every start (Design.md, Setup and repair).
class SetupChecker
{
public:
  explicit SetupChecker(Axodox::Infrastructure::dependency_container* container);

  SetupStatus Check() const;  //child sessions enabled, leftover RunOnce entry, folders, integrations
  void RepairPerUser();       //unelevated fixes
};

//The machine-wide steps, run by SecondShell.exe --setup in an elevated process.
class MachineSetup
{
public:
  void EnableChildSessions();
};

//Starts SecondShell.exe --setup elevated (UAC) and waits for it.
class ElevatedSetupLauncher
{
public:
  bool Run(HWND owner);  //false if the user declined
};

//Removes what SecondShell added (Design.md, Uninstall).
class Uninstaller
{
public:
  explicit Uninstaller(Axodox::Infrastructure::dependency_container* container);

  void Run(const UninstallOptions& options);
};
```

### 6.3 Session (`SecondShell::Session`)

```cpp
//The child session password: Credential Manager entry, DPAPI-encrypted with application entropy.
class CredentialStore
{
public:
  std::optional<Credentials> Load() const;
  void Save(const Credentials& credentials);
  void Delete();
};

//The Windows credential prompt (CredUIPromptForWindowsCredentials).
class CredentialPrompt
{
public:
  std::optional<Credentials> Show(HWND owner, std::wstring_view message) const;
};

//The policy: saved credentials, else the prompt; saves entered ones after a successful sign-in, deletes rejected ones.
class CredentialProvider
{
public:
  explicit CredentialProvider(Axodox::Infrastructure::dependency_container* container);

  void SetPromptOwner(HWND owner);
  void ApplyTo(RdpHostWindow& window);  //on the RDP thread, before connecting
  void OnSignInSucceeded();
  void OnSignInFailed(long disconnectReason);  //0: cancelled or timed out
};

//Persistent user settings (display and capture defaults, shell, inline capture limit), a described type stored in the data folder.
class SettingsStore
{
  Axodox::Infrastructure::event_owner _events;

public:
  explicit SettingsStore(Axodox::Infrastructure::dependency_container* container);

  Settings Current() const;
  void Update(const std::function<void(Settings&)>& change);

  Axodox::Infrastructure::event_publisher<SettingsStore*> Changed;
};

//WTS functions for child sessions.
class ChildSessionApi
{
public:
  bool IsEnabled() const;
  std::optional<uint32_t> ChildSessionId() const;
  void LogOff(uint32_t sessionId);
};

//The hidden Win32 window hosting the RDP ActiveX control. Never shown, never minimized.
class RdpHostWindow
{
public:
  RdpHostWindow();

  HWND Handle() const;
  void SetCredentials(std::wstring_view userName, std::wstring_view domain, std::wstring_view password);
};

//The RDP client: configures and connects to the child session, reports its state. Knows nothing about drivers or agents.
class RdpClient
{
  Axodox::Infrastructure::event_owner _events;

public:
  explicit RdpClient(Axodox::Infrastructure::dependency_container* container);

  void Connect(const Credentials& credentials, const DisplaySettings& display);
  void Disconnect();
  void UpdateDisplay(const DisplaySettings& display);  //UpdateSessionDisplaySettings
  bool IsConnected() const;

  Axodox::Infrastructure::event_publisher<RdpClient*> Connected;
  Axodox::Infrastructure::event_publisher<RdpClient*> LoginCompleted;
  Axodox::Infrastructure::event_publisher<RdpClient*, const DisconnectEventArgs&> Disconnected;
};

//Writes and removes the driver's RunOnce entry.
class DriverLauncher
{
public:
  explicit DriverLauncher(Axodox::Infrastructure::dependency_container* container);

  void Arm();     //before connecting to a new session
  void Disarm();  //resume, failure, startup cleanup
  bool IsArmed() const;
};

//Session lifecycle: start, resume or reset, sign-in, driver startup, shutdown. The only class that sequences these steps.
class SessionManager
{
  inline static const Axodox::Infrastructure::logger _logger{ "SessionManager" };
  Axodox::Infrastructure::event_owner _events;

public:
  explicit SessionManager(Axodox::Infrastructure::dependency_container* container);

  SessionState State() const;  //Stopped, Connecting, SigningIn, StartingDriver, Ready, Stopping, Failed
  SessionInfo Info() const;    //session ID, display, driver connected
  bool HasExistingSession() const;

  void Start(const SessionOptions& options, ExistingSessionPolicy policy, const CancellationToken& cancellation);
  void Reset(const CancellationToken& cancellation);
  void Shutdown();  //always logs the session off

  Axodox::Infrastructure::event_publisher<SessionManager*, SessionState> StateChanged;
};

//Display settings of the session (Set-Display / Get-Display).
class DisplayController
{
public:
  explicit DisplayController(Axodox::Infrastructure::dependency_container* container);

  DisplaySettings Get() const;
  void Set(const DisplaySettings& display);
};

//The host's side of the driver connection: accepts the driver on its pipe, applies the session guard
//(pipe client session ID == child session ID), and sends actions.
class IDriverLink
{
public:
  virtual bool IsConnected() const = 0;
  virtual bool WaitForConnection(std::chrono::milliseconds timeout, const CancellationToken& cancellation) = 0;
  virtual Rpc::RpcResult<Protocol::ExecuteActionResponse> Execute(const Protocol::ExecuteActionRequest& request, const CancellationToken& cancellation) = 0;
  virtual void ResetInput() = 0;
  virtual void ForwardUserInput(const Protocol::ForwardUserInputNotification& input) = 0;
  virtual void SetClipboard(const Actions::ClipboardContent& content) = 0;
  virtual void SetClipboardSync(bool isEnabled) = 0;
  virtual ~IDriverLink() = default;

  //Events, declared on the base: Connected, Disconnected, ActionCompleted, ClipboardChanged
};

class DriverLink : public IDriverLink;  //named pipe messaging server on PipeNames::Driver(), JsonRpcPeer per connection
```

### 6.4 Agent (`SecondShell::Agent`)

```cpp
//Answers the MCP server's requests (session/initialize, sequence/run, ...), delegating to the services.
class McpRequestHandler
{
public:
  explicit McpRequestHandler(Axodox::Infrastructure::dependency_container* container);

  void Start();  //named pipe messaging server on PipeNames::Mcp(), JsonRpcPeer per connection

private:
  Rpc::RpcResult<Protocol::InitializeSessionResponse> InitializeSession(const Protocol::InitializeSessionRequest& request, const CancellationToken& cancellation);
  Rpc::RpcResult<Protocol::RunSequenceResponse> RunSequence(const Protocol::RunSequenceRequest& request, const CancellationToken& cancellation);
  //...one method per tool
};

//Runs one sequence: order, async actions and Control.Join, timeouts, stop on failure, releasing input after a failure,
//user interruption. Reports each step to ActionLog and VisualizationFeed.
class SequenceRunner
{
public:
  explicit SequenceRunner(Axodox::Infrastructure::dependency_container* container);

  Protocol::RunSequenceResponse Run(const Protocol::RunSequenceRequest& request, const ProgressCallback& progress, const CancellationToken& cancellation);
};

//Who controls the session: the agent, or the user (interactive mode, Stop).
class AgentControl
{
  Axodox::Infrastructure::event_owner _events;

public:
  ControlMode Mode() const;  //Agent, UserInteractive, Stopped
  bool IsAgentAllowed() const;
  void Stop();               //cancels the running sequence, releases input
  void SetInteractive(bool isInteractive);
  void GiveBackControl();
  const CancellationToken& AgentCancellation() const;

  Axodox::Infrastructure::event_publisher<AgentControl*, ControlMode> ModeChanged;
};

//Captures: inline limit per sequence, file paths, cleanup of old session folders.
class CaptureStore
{
public:
  explicit CaptureStore(Axodox::Infrastructure::dependency_container* container);

  CaptureDestination Decide(const Actions::CaptureOutput& requested, uint32_t inlineCountSoFar) const;
  std::filesystem::path Save(std::span<const uint8_t> image, const CaptureFileName& name) const;
  void CleanUpOldSessions();
};

//Every agent action, with sensitive text redacted and clipboard content truncated.
class ActionLog
{
  Axodox::Infrastructure::event_owner _events;

public:
  explicit ActionLog(Axodox::Infrastructure::dependency_container* container);

  void Add(const Actions::Action& action, const Actions::ActionResult* result);
  std::vector<ActionLogEntry> Entries() const;

  Axodox::Infrastructure::event_publisher<ActionLog*, const ActionLogEntry&> EntryAdded;
};

//Turns executed actions into visualization events for the overlay.
class VisualizationFeed
{
  Axodox::Infrastructure::event_owner _events;

public:
  void OnActionStarted(const Actions::Action& action);
  void OnActionFinished(const Actions::Action& action, const Actions::ActionResult& result);

  Axodox::Infrastructure::event_publisher<VisualizationFeed*, const VisualEvent&> EventRaised;
};

//Maps the user's pointer and keyboard input in the session view to session pixels and forwards it to the driver.
class InteractiveInputForwarder
{
public:
  explicit InteractiveInputForwarder(Axodox::Infrastructure::dependency_container* container);

  void OnPointer(const PointerInput& input, const ViewTransform& transform);
  void OnKey(const KeyInput& input);
};

//Syncs the user's and the session's clipboard in both directions, only in interactive mode.
class ClipboardSync
{
public:
  explicit ClipboardSync(Axodox::Infrastructure::dependency_container* container);  //subscribes to AgentControl::ModeChanged
};
```

### 6.5 Rendering (`SecondShell::Rendering`)

```cpp
//Subscribes to the driver's frames (frames/subscribe) and reads the newest one from the shared ring (SharedFrameReader).
class FrameReceiver
{
public:
  explicit FrameReceiver(Axodox::Infrastructure::dependency_container* container);

  SharedFrameLock LockLatest();
  Size FrameSize() const;
};

//WinUI 3 SwapChainPanel adapter for Axodox.Common's swap chains (via ISwapChainPanelNative).
class WinUiSwapChain : public Axodox::Graphics::SwapChain
{
public:
  WinUiSwapChain(const Axodox::Graphics::GraphicsDevice& device, const winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel& panel);
};

//Draws the newest frame, letterboxed into the panel, then the overlay; runs on the render thread.
class SessionRenderer
{
public:
  explicit SessionRenderer(Axodox::Infrastructure::dependency_container* container);

  void Attach(const winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel& panel);
  void Detach();
  ViewTransform Transform() const;  //panel coordinates <-> session pixels
};

//Draws the registered overlay elements with Direct2D.
class OverlayRenderer
{
public:
  explicit OverlayRenderer(Axodox::Infrastructure::dependency_container* container);  //resolves registry<OverlayElement>

  void Draw(ID2D1DeviceContext* context, const ViewTransform& transform, std::chrono::steady_clock::time_point now);
};

//One kind of visual element; subscribes to VisualizationFeed itself.
class OverlayElement
{
public:
  virtual void Update(std::chrono::steady_clock::time_point now) = 0;
  virtual void Draw(ID2D1DeviceContext* context, const ViewTransform& transform) = 0;
  virtual ~OverlayElement() = default;
};
```

Overlay elements: `CursorOverlay` (pointer, buttons, wheel, click animations), `TouchOverlay`, `KeystrokeOverlay` (keys and the scrolling keystroke list), `GamepadOverlay`, `CaptureFlashOverlay`, `ActivityOverlay` (started processes, clipboard actions).

### 6.6 AI client integration (`SecondShell::Integrations`)

```cpp
//One AI client: detection, status, and adding / updating / removing the SecondShell entry. Registered in registry<AiClientIntegration>.
class AiClientIntegration
{
public:
  virtual std::string_view Name() const = 0;
  virtual bool IsInstalled() const = 0;
  virtual IntegrationStatus Status() const = 0;      //NotConfigured, Configured, Outdated (old path)
  virtual IntegrationChange PreviewAdd() const = 0;  //what will change, for the confirmation dialog
  virtual void Add() = 0;
  virtual void Remove() = 0;
  virtual bool NeedsRestart() const = 0;
  virtual ~AiClientIntegration() = default;
};

//Edits a JSON config file safely: backup, change only the SecondShell entry, keep everything else.
class JsonConfigEditor
{
public:
  explicit JsonConfigEditor(std::filesystem::path path);

  bool HasEntry(std::string_view section, std::string_view name) const;
  void SetEntry(std::string_view section, std::string_view name, const Axodox::Json::json_value& entry);
  void RemoveEntry(std::string_view section, std::string_view name);
  void Save();  //writes <file>.bak first
};

//Runs a client's command-line tool (claude, code) and captures its result.
class CliRunner
{
public:
  CliResult Run(std::string_view executable, std::span<const std::string> args, std::chrono::milliseconds timeout) const;
};

//The registered integrations; used by the "Add to AI apps" page, --integrate, the start checks and the uninstaller.
class IntegrationManager
{
public:
  explicit IntegrationManager(Axodox::Infrastructure::dependency_container* container);

  std::vector<AiClientIntegration*> Integrations() const;
  AiClientIntegration* Find(std::string_view name) const;
  void UpdateOutdated();
};
```

Integrations: `ClaudeDesktopIntegration`, `ClaudeCodeIntegration` (CLI), `VsCodeIntegration` (CLI or config), `VisualStudioIntegration`, `CursorIntegration`, `WindsurfIntegration`, `LmStudioIntegration`, `GeminiCliIntegration`, `CodexCliIntegration` (TOML: `TomlConfigEditor`). The JSON-file ones share `JsonFileIntegration`, configured with path, section name and entry format.

### 6.7 User interface (XAML)

Runtime classes in `winrt::SecondShell::implementation`, declared in IDL. Views contain layout and bindings only. View models implement `INotifyPropertyChanged`, take a `ViewModelContext`, resolve their services from it, and expose actions as methods that views bind to (no `ICommand` objects). Service events reach them on background threads and are marshalled to the UI thread as described in Threads.

| Kind | Classes | Purpose |
|---|---|---|
| Window | `MainWindow` | Navigation (session, log, AI apps, settings), status bar, title bar |
| Views | `SessionPage` | `SwapChainPanel` with the session, toolbar (mode toggle, Stop, give back control, reset, resolution) |
| | `ActionLogPage` | The action log, filterable |
| | `IntegrationsPage` | "Add to AI apps": clients, status, add / update / remove, copy snippet |
| | `SettingsPage` | Display and capture defaults, shell, inline capture limit, forget password |
| Controls | `StatusBar` | Setup state, session state, agent mode |
| Dialogs | `RepairDialog` | Missing machine-wide setup; starts the elevated repair |
| | `ExistingSessionDialog` | Resume or reset a session left over from before |
| | `CloseSessionDialog` | Confirms closing while the agent is working (logs the session off) |
| | `IntegrationPreviewDialog` | Shows the change to an AI client's config before writing it |
| View models | `MainViewModel`, `SessionViewModel`, `ActionLogViewModel`, `IntegrationsViewModel`, `SettingsViewModel`, `StatusViewModel` | State and actions for the views |
| Converters | `BooleanToVisibilityConverter`, `SessionStateToTextConverter`, `SessionStateToBrushConverter`, `ControlModeToTextConverter`, `IntegrationStatusToTextConverter` | Value conversion for bindings |

### 6.8 Stage 5 implementation notes

- **Driver** (`SecondShell.Driver`): `main.cpp` is the composition root. `HostConnection::Open` opens a `named_pipe_messaging_client` on the driver pipe and waits for its first connection; the `connected` handler refuses a host in the driver's own session by closing the client before the channel opens (the driver's half of the guard), and the client is closed when the host disconnects, so the driver never reconnects; `ActionExecutor` adds its handlers; `HostConnection::Hello` starts the peer and asks the host to accept the driver; only then does `ScreenCapturer` start. The handlers also refuse to run before acceptance. Mouse, keyboard and capture actions are implemented (`MouseActionHandler`, `KeyboardActionHandler`, `CaptureActionHandler`); other kinds fail with "not available yet". Presses and releases go through `InputStateTracker`, which releases and reports what is held. `ScreenCapturer` keeps the newest frame; `CaptureEncoder` scales and encodes on the CPU (WIC). The driver logs to `driver.log` (Axodox `logger::file`).
- **Host session**: `RdpClient` runs `RdpHostWindow` (the hidden ATL-hosted control, `#import`ed in one file) on a `StaThread`. Credentials: `CredentialProvider` loads them from `CredentialStore` or asks with `CredentialPrompt` (owned by the main window, pre-filled with the current user), hands them to `RdpHostWindow::SetCredentials` (the only place that touches the control), saves entered ones after `OnLoginComplete` and deletes saved ones on a rejecting disconnect reason (2055, 3847, 4615). Passwords live in `Secret`, which is move-only and wiped on destruction; `SessionManager` reports every sign-in outcome, so an entered password is dropped on timeout and cancellation too. `SessionManager` sequences start and shutdown: setup check, a left-over child session only with `existingSession: Reset` (it may belong to another tool), `DriverLauncher::Arm`, sign-in, `DriverLink::Expect(childSessionId)`, wait for the driver. Cleanup logs off only a child session this host connected to.
- **Session guard (host)**: `DriverLink` accepts one driver whose pipe session ID equals the expected child session ID; with no expected session, every driver is refused.
- **Sequencing**: `SequenceRunner` runs Control.* itself and the rest on the driver, one sequence at a time; async actions are tracked by action ID until `Control.Join`, whose failure is the first failed async action. A failed step stops the sequence, input is released, and held input is reported. `CaptureStore` decides inline or file (inline beyond the limit goes to file, with a message), forces PNG inline, and saves files. `AgentControl` and `ActionLog` are minimal until the UI (stage 7).
- **Not yet**: `display/set` (stage 6), resuming a session, restarting a crashed driver, the setup checker beyond "child sessions enabled", settings storage.

## 7. Key flows

### Starting a session (`Initialize-Session`)

1. `McpServer::CallTool` → `SessionToolSource::InitializeSession` → `HostClient` (starts the host if needed via `HostLauncher`) → `session/initialize` → `McpRequestHandler`.
2. `SessionManager::Start`: `SetupChecker` must report child sessions enabled; otherwise a `SetupRequired` error.
3. If `ChildSessionApi` reports an existing child session, the policy decides between resume and reset (`ExistingSessionDialog` when the user starts it from the window).
4. New session: `DriverLauncher::Arm`, `RdpClient::Connect`, which has `CredentialProvider::ApplyTo` use `CredentialStore::Load` (or `CredentialPrompt`).
5. `RdpClient::LoginCompleted` → state `StartingDriver` → `IDriverLink::WaitForConnection` (the driver started by RunOnce passes the session guard) → state `Ready`.
6. `FrameReceiver` subscribes to the driver's frames; `SessionPage` shows them.

### Running a sequence (`Run-Sequence`)

1. `SessionToolSource::RunSequence` receives validated arguments (the steps were parsed into `Action`s by their descriptors) and sends `sequence/run` to the host.
2. `SequenceRunner` checks `AgentControl::IsAgentAllowed`, then for each step logs it (`ActionLog`), reports it (`VisualizationFeed`), and either handles it itself (`Control.*`) or sends it to the driver (`IDriverLink::Execute`).
3. The driver's `ActionExecutor` dispatches the step by kind and type to its handler, which uses the injectors, capturer, window, process or clipboard services and returns the result.
4. `CaptureStore` decides inline vs. file for each capture.
5. On failure, timeout or user interruption: `IDriverLink::ResetInput`, and the response reports the failed step and the input that was held.
6. `RunSequenceResult` becomes the MCP result: text, structured content, inline images.

### Driver startup and the session guard

1. `DriverApplication::Run` → `HostConnection::Connect` connects to `PipeNames::Driver()`; no pipe → exit.
2. `driver/hello`: `DriverLink` compares the pipe client's session ID with `ChildSessionApi::ChildSessionId()` and accepts or rejects.
3. Rejected → the driver exits. Accepted → it starts `ActionExecutor`, `ScreenCapturer` and `FramePublisher`.

## 8. Tests (`SecondShell.Tests`)

Unit tests in the same framework as Axodox.Common's tests, each with its own local `dependency_container`, for the code that doesn't need a child session:

- Actions: parsing by discriminator, validation errors, generated schemas.
- `JsonRpcPeer`: requests, responses, notifications, errors, cancellation, over an in-memory transport.
- `McpServer` and `ToolSource`: recorded MCP exchanges with test tools (`McpTestClient` over the in-memory transport); `SessionToolSource` against a fake host on a pipe of its own; the tool schemas as a snapshot (`Tools.json`).
- MCP probe: runs `SecondShell.Mcp.exe` and speaks MCP over its stdio (expects no host running).
- `SequenceRunner`: async actions, `Control.Join`, timeouts, stop on failure, interruption, with a fake `IDriverLink`.
- `JsonConfigEditor` and the integrations: merging into existing configs, backups, removal.
- `KeyNames`, `ShellCommandBuilder`, `CaptureStore` decisions, `ClipboardFormatConverter`.

Session-level behaviour (RDP, driver startup, input injection, capture, shared frames) is tested with the scripts from Findings.md, later replaced by an integration test harness.

## 9. Proposed additions to Axodox.Common

General-purpose pieces SecondShell needs, which fit Axodox.Common better than SecondShell. They follow Axodox.Common's conventions (snake_case, file per type, umbrella headers).

| Addition | Module | Contents | Needed for |
|---|---|---|---|
| **Named pipe messaging** (implemented in stage 2, branch `named_pipe_messaging`) | `Networking/Messaging/Pipes` | `named_pipe_messaging_channel` (a `messaging_channel` over a pipe, with `remote_peer()`: process and session ID), `named_pipe_messaging_server`, `named_pipe_messaging_client` (connects and reconnects every second, raising `connected` before the channel opens; `close()` from that handler refuses a server; refuses pipes another user created), `Security/win32_user_security` (current-user-only access list), `PIPE_REJECT_REMOTE_CLIENTS`; the first pipe instance claims the name, so another process cannot take it over | All communication between SecondShell's processes |
| **Shared textures with keyed mutex** | `Graphics/Textures` | Shared NT-handle textures with `IDXGIKeyedMutex`, a lock type releasing the mutex when destroyed, opening by handle and by name, handle duplication into another process (similar to Asgard's `GraphicsMutex` / `GraphicsLock`) | Shared frames (section 3.6) |
| **JSON-RPC 2.0 peer** | `Json` or `Networking` | A generic version of `JsonRpcPeer`: typed handlers keyed by descriptor name, requests with ID matching, notifications, cancellation, over any text message transport | All three links; also useful for other JSON-RPC protocols |
| **WinUI 3 swap chain** | `Graphics/Swap Chains` | `WinUiSwapChain` for `Microsoft::UI::Xaml::Controls::SwapChainPanel`, the counterpart of the UWP-only `XamlSwapChain` | The host's session view |

The JSON-RPC peer is built in SecondShell first and proposed once it has proven itself.
