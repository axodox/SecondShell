# Moving to the reconnecting pipe client

Axodox.Common removed `named_pipe_messaging_client::connect`, the static one-shot connect used by the driver and the MCP server. Connections now go through a `named_pipe_messaging_client` instance, which owns the channel and reconnects by itself. The same change destroys the old channel outside the client's lock, so a `message_received` handler may call the client while a connection drops.

`named_pipe_messaging_channel::peer_process_id()` and `peer_session_id()` were also replaced by `remote_peer()`, which returns a `named_pipe_peer_description { process_id, session_id }`.

- [x] Update the callers: `PipeTransport`'s constructor and `HostConnection::Open` / `Hello`; the latter two are rewritten below anyway.

## What the client provides

- `client.open()` starts a background thread, which tries to connect immediately, and after a failure or a disconnect every second.
- `connected(client, channel)` is raised on that thread **before the channel opens**. Create the `PipeTransport` in this handler, with `static_cast<named_pipe_messaging_channel*>(channel)`, so no message is missed. `PipeTransportTests.PeersTalkOverAPipe` already does this.
- The channel pointer is valid until `disconnected`. `PipeTransport::OnChannelDisconnected` already drops it then, so `PipeTransport` needs no change.
- `connected` is raised again after every reconnect.
- Each failed attempt is logged by `messaging_client` at error severity, once a second. Quieter logging is planned in Axodox.Common, but not done yet.
- A failure reason is no longer returned to the caller; it is only in that log.

## Driver: `HostConnection`

- [x] Replace `unique_ptr<named_pipe_messaging_channel> _channel` with a `named_pipe_messaging_client` member, declared **before** `_transport` and `_peer`, so it is destroyed after them.
- [x] `Open(timeout)`: subscribe to `connected`, call `open()`, then wait for the handler with the timeout instead of the retry loop. The client's retries also cover the case the loop was for, the host being busy accepting another connection.
- [x] In the `connected` handler:
  - Compare `channel->peer_session_id()` with this process's session, as `Open` does now, and store the result.
  - Create `_transport` and `_peer` only for a host in another session.
  - Signal `Open`.
- [x] Accept only the first connection. After the host disconnects, the client may reconnect before `main` exits, and the handler must not replace `_transport` and `_peer` while they are in use. Use a flag set by the first call.
  - *Done differently:* the transport's `Closed` handler closes the client, so it never reconnects, and no flag is needed; the host also never sees a stray second connection.
- [x] Reject a host in this session by calling `client->close()` in the `connected` handler: the channel is then not opened at all, and the client stops reconnecting. `Open` returns false and `main` exits.
  - The pipe itself was opened, so the host may see a client connect and leave, but no message is exchanged.
  - The host's half of the session guard refuses a driver in its own session anyway.
- [x] `Hello`: remove `_channel->open()`, as the client opens the channel itself. Take the process ID for the log from `_transport->PeerProcessId()`.
- [x] Keep `_peer->Start()` in `Hello`, after `ActionExecutor` added its handlers.
  - `JsonRpcPeer` only subscribes to the transport in `Start`, so anything arriving earlier is dropped.
  - That is fine: the host sends nothing before `driver/hello`.

## MCP server: `HostClient`

- [x] Replace `Connect()` and `Connection::Channel` with a `named_pipe_messaging_client` member, declared before `_connection`. `Connection` keeps `Transport` and `Peer`.
- [x] In the `connected` handler, build a new `Connection`:
  - `PipeTransport` from the cast channel, `JsonRpcPeer`, `AddNotificationHandler(OnSequenceProgress)`, `Start()`.
  - Store it in `_connection` under `_mutex`, replacing the previous one, and signal waiters.
  - Requests still holding the previous connection finish with it, as now; its transport is already closed.
- [x] `Request`: take `_connection` under the lock. Fail with `SessionNotActive` if there is none, or its transport is closed. Do not try to connect there.
- [x] `IsRunning`: true if `_connection` exists and its transport is open, without a connection attempt.
- [x] `Start`:
  - Return if connected.
  - Otherwise launch the host and wait for `connected`, keeping the 30-second deadline and the cancellation, instead of polling `Connect()`.
- [x] Decide when to open the client: *on first use, by `Start`, `IsRunning` or `Request`* (a host may already be running when the MCP server starts, so the first `Check-Session` or `Run-Sequence` must find it).
  - **In the constructor**: simplest, but the client retries for as long as there is no host, and the MCP server often runs without one until `Initialize-Session`. That means an error in the log every second, until the quieter logging is done in Axodox.Common.
  - **On the first `Start` or `IsRunning`**: no retries before SecondShell is used. After the host exits, the client still retries until it is back.
- [x] Avoid starting a second host. Right after `open()`, the client may not have connected yet even though a host is running, so `Start` would launch another one.
  - Either wait briefly (about one second) for `connected` after opening before launching,
  - or check what a second host does: the first pipe instance claims the name, so its pipe server cannot open.
  - *Done:* the first use waits up to a second for the client's first, immediate attempt. A second host would exit anyway, as its pipe server cannot open.

## Tests

- [x] `DriverLinkTests`: `TestDriver` uses `connect`. Switch it to a client with a `connected` handler and a ready event, as in `PipeTransportTests.PeersTalkOverAPipe`.
- [x] `SessionToolSourceTests`: `HostClient` now connects asynchronously. Tests that expect it to see the `FakeHost` immediately need to wait for the connection. The first attempt is immediate; after a failure the next one comes a second later.
  - *No change needed:* the first use waits for the first attempt. The pipe tests now take a fresh name each (`UniquePipeName`): names from `GetTickCount64` repeated within a tick, and the refactored server cannot open while an instance of the same name is still closing.
- [x] Add tests for `HostClient`:
  - `Start` connecting to a host that appears later.
  - Requests failing with `SessionNotActive` after the host disconnects, and working again once it is back.
  - *Done* through the tools with the `FakeHost` (`AHostStartedLaterIsFound`, `RequestsWorkAgainOnceTheHostIsBack`); `Start` itself launches the real host, so `SessionProbeTests` covers it.

## Docs and build

- [x] Update `Build/packages.config` to an Axodox.Common package with these changes (currently `1.0.120`). *Done:* `1.0.124-named-pipe-messaging`, built locally (see ImplementationPlan.md, notes).
- [x] `docs/Architecture.md`:
  - The *Connections* paragraph (around line 768) describes `HostClient` connecting with `named_pipe_messaging_client::connect`.
  - The Axodox.Common table (around line 1245) lists "`connect` once".
- [x] `docs/ImplementationPlan.md` (line 27) lists `connect` among the stage 4 fixes. Keep it as history, or note that it was replaced.
