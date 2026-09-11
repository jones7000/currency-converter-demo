# Architecture

This document describes the system architecture for the Currency Converter application.

## 1. System Overview

```
┌─────────────────────┐        gRPC-Web         ┌──────────────────┐        native gRPC        ┌──────────────────────┐        HTTPS/REST        ┌───────────────────┐
│   Angular Client    │  (HTTP/1.1 or HTTP/2)   │   Envoy Proxy    │        (HTTP/2)           │   C++ gRPC Server    │      (JSON)              │  frankfurter.dev  │
│   (Material UI)     │ ───────────────────────▶│  (transcoding +  │ ────────────────────────▶ │   ("Backend")        │ ───────────────────────▶ │  (external public │
│   "dumb client"     │◀─────────────────────── │   CORS)          │◀───────────────────────── │   + in-memory cache  │◀─────────────────────────│   API)            │
└─────────────────────┘                         └──────────────────┘                           └──────────────────────┘                          └───────────────────┘
```

Three processes, three responsibilities:

| Component | Responsibility | Explicitly must **not** do |
|---|---|---|
| Angular client | Render UI, collect user input, display server responses | Any conversion math, date-range validation logic, or business rules |
| Envoy proxy | Translate gRPC-Web ⇄ gRPC, handle browser CORS | Anything stateful or business-related |
| C++ backend | Validate input, talk to frankfurter.dev, cache, convert, compute history | Rendering, UI concerns |

The proxy is the one piece not requested explicitly by `task.md`. It exists purely because of a
technical constraint, explained in §2.

## 2. Decision: Protocol — gRPC over REST

`task.md` explicitly asks to **choose and evaluate** gRPC vs. REST, while still providing a
`.proto` file as the mandatory contract in both cases. This project uses **gRPC** end-to-end
(gRPC-Web on the browser side). Rationale, in order of relevance to the application:

1. **Protocol skills in scope.** `task.md` lists gRPC, TCP/UDP and WebSocket as core protocol
   requirements. Choosing REST would mean using the `.proto` file only as documentation and never
   exercising actual gRPC machinery (codegen, streaming-capable stubs, status codes) — a missed
   opportunity to put that machinery to use.
2. **Contract-first, compile-time-checked.** With gRPC, the `.proto` file *is* the API — both the
   C++ server and the Angular client generate stubs from the same source of truth, so client/server
   drift becomes a compile error instead of a runtime bug. This is a direct, demonstrable
   instance of the "safety-critical mindset" principle from `task.md`: an entire class
   of integration bugs (typo'd JSON field, wrong type) is eliminated before the code runs.
3. **Honest trade-off, not free.** Browsers cannot speak native gRPC (HTTP/2 trailers used for
   status/metadata aren't exposed via `fetch`/`XHR`). This means gRPC-Web is required, which in
   turn requires a translating proxy (Envoy) between Angular and the C++ server, because grpc-web's
   wire format is not identical to native gRPC framing. That is a third infrastructure component,
   extra local-dev wiring, and one more hop to debug. This is called out explicitly rather than
   hidden, because acknowledging a trade-off you chose to accept — and mitigating it (§6, Nix
   flake) — is itself the "systems thinking" the caching section of `task.md` asks for elsewhere.
4. **REST was the "path of least resistance" alternative** (skip the proxy, skip the codegen
   friction) but would have made the `.proto` file cosmetic. Given `task.md` explicitly frames
   protocol choice as something to reason about rather than default on, REST was rejected on that
   basis, not on technical merit — REST would have been a perfectly valid, simpler choice for an
   app this size.

**Consequence for the contract:** the provided `.proto` template is extended with RPC method
signatures (the template only defined messages). See [`api.md`](api.md).

## 3. Decision: Backend HTTP Client — POCO over Boost

`task.md` asks to evaluate POCO vs. Boost for the "HTTP Client/Server" role. Because gRPC (via
`grpc++`, built on its own core, not on POCO/Boost) already owns the server-side transport to the
Angular client, the only remaining HTTP role is the **outbound REST client to frankfurter.dev**.
That is the concrete comparison made here.

| Criterion | POCO (`Poco::Net`, `Poco::JSON`, `Poco::Util`) | Boost (`Boost.Beast` + `Boost.Asio`) |
|---|---|---|
| HTTPS client boilerplate | `HTTPSClientSession` is a handful of calls | Beast requires manual TLS stream setup, buffer management, request/response parsing |
| JSON parsing | Built in (`Poco::JSON::Parser`) | Not included; would pull in a third dependency (e.g. `nlohmann/json`) |
| Config file (JSON) + CLI args | `Poco::Util::ServerApplication` + `Poco::Util::JSONConfiguration` load and merge both **out of the box** — this is exactly the `max_age`/`max_elements` config requirement in `task.md` | No equivalent; would be hand-rolled (argv parsing + a JSON library + manual merge logic) |
| Logging / app lifecycle | `Poco::Util::ServerApplication` gives structured logging and signal handling for free | Not provided; would be hand-rolled |
| Async/low-level control | Coarser-grained | Fine-grained control via `Boost.Asio` (relevant if the server later needed to be fully async) |
| Ecosystem weight | Single cohesive library covering Net+JSON+Util | Header-only, but multiple sub-libraries need to be composed |

**Decision: POCO.** The deciding factor is not raw capability (Beast is more powerful for
high-concurrency async servers) but **fit for this project's actual bottleneck** — `task.md`
calls out the backend caching mechanism as the area where C++ design skill matters most. Every
hour not spent hand-rolling HTTP/JSON/config plumbing with Beast is an hour available for the
cache's data structures, thread-safety, and layering — the part that matters most here. POCO also
solves the config-loading requirement (JSON file *or* CLI args) as a side effect of the
app-framework choice, rather than as bespoke code, which keeps the codebase leaner from a Clean
Code standpoint (less incidental complexity).

The frankfurter.dev calls are synchronous/blocking (`Poco::Net::HTTPSClientSession`). This is a
deliberate choice, not an oversight: `grpc++`'s default sync server API dispatches each RPC to a
worker thread from a pool, so a blocking outbound call simply occupies that one worker thread —
there is no event loop to stall. This is documented explicitly in §5 (Concurrency) so it reads as
a deliberate choice rather than an oversight.

## 4. Backend Layering

Layers, dependency direction strictly top-to-bottom (no upward dependencies — Dependency
Inversion Principle applied via interfaces at the two infrastructure boundaries):

```
┌────────────────────────────────────────────────────────────────┐
│  main.cpp (composition root: Poco::Util::ServerApplication)    │
└────────────────────────────────────────────────────────────────┘
                │ wires concrete implementations into services
                ▼
┌────────────────────────────────────────────────────────────────┐
│  grpc/           CurrencyServiceImpl                           │  Transport layer.
│                  (extends generated CurrencyService::Service)  │  Maps proto ⇄ domain types,
└────────────────────────────────────────────────────────────────┘  maps exceptions ⇄ grpc::Status.
                │ calls
                ▼
┌────────────────────────────────────────────────────────────────┐
│  application/     CurrencyCatalogService                       │  Business logic / orchestration.
│                    ConversionService                           │  Validates input, applies bounds,
│                    HistoryService                              │  decides cache-hit vs. fetch.
│                    ConstraintsService                          │  Depends only on interfaces below.
└────────────────────────────────────────────────────────────────┘
                │ depends on (interfaces)          │ depends on (interfaces)
                ▼                                  ▼
┌───────────────────────────────┐     ┌────────────────────────────────────┐
│  cache/    ICache<K,V>        │     │  external/  IExchangeRateProvider  │
│            TimeAndSizeBoundedCache│ │             FrankfurterClient      │  Infrastructure layer.
│            (implementation)   │     │             (POCO-based impl)      │  Swappable, mockable.
└───────────────────────────────┘     └────────────────────────────────────┘
                │                                   │
                ▼                                   ▼
┌────────────────────────────────────────────────────────────────┐
│  domain/          Currency, ExchangeRate, DateRange, Money,    │  Plain value types +
│                    domain exceptions                           │  domain exceptions. No
└────────────────────────────────────────────────────────────────┘  framework dependency.
```

Why this shape:

- **Single Responsibility per layer**, matching `task.md`'s "clear layer
  architecture" bullet almost verbatim (HTTP layer / business logic / external API client /
  cache, each independently swappable and, if tests were in scope, independently mockable behind
  `IExchangeRateProvider` and `ICache`).
- **The gRPC layer is intentionally thin.** It only translates; if the protocol were ever swapped
  back to REST, only this layer and `main.cpp` would change — a concrete argument for why gRPC was
  a safe choice under Dependency Inversion, addressing the "REST would have been simpler" trade-off
  from §2 with an actual mitigation, not just a claim.
- **Domain types are separate from generated proto types.** Proto messages are a wire format, not
  a domain model — collapsing them would leak transport concerns (e.g. `repeated`, proto3 default
  values) into business logic. The mapping cost is a few small, obvious functions in the gRPC
  layer, paid once, in exchange for a business layer with no framework awareness.

## 5. Caching — Design and Concurrency

This is the area `task.md` calls out as most central, so the reasoning is spelled out in full.

### 5.1 Requirements recap (from `task.md`)

- All historical-exchange-rate queries must be cached.
- Cache hit ⇒ served from cache, no external call.
- **Time-based eviction:** entries older than a configurable `max_age` are removed.
- **Size-based eviction:** a configurable `max_elements` capacity; exceeding it deletes the
  *oldest* entry.
- Both parameters configurable via JSON file or CLI args.

### 5.2 Data structure

Classic **LRU + TTL combination**, implemented once as a generic, reusable component:

```cpp
template <typename Key, typename Value>
class TimeAndSizeBoundedCache {
    struct Entry { Key key; Value value; std::chrono::steady_clock::time_point insertedAt; };

    std::list<Entry> entries_;                                    // front = most recently used
    std::unordered_map<Key, std::list<Entry>::iterator> index_;   // O(1) lookup
    std::mutex mutex_;
    std::chrono::minutes maxAge_;
    std::size_t maxElements_;
};
```

- `unordered_map` gives O(1) key lookup; `list` gives O(1) move-to-front and O(1) eviction of the
  tail — the textbook LRU pairing, chosen because it is the simplest structure that satisfies both
  eviction strategies without a second index. This directly follows `task.md`'s call to
  "demonstrate your choice of data structures."
- **Size-based eviction** = classic LRU: on insert, if `entries_.size() > maxElements_`, pop the
  tail (the least-recently-used / oldest entry).
- **Time-based eviction** = lazy expiry checked on access (compare `now - insertedAt` against
  `maxAge_`) *plus* a periodic sweep (background thread, interval derived from `maxAge_`) so that
  entries that are never looked up again are still reclaimed — otherwise a cache that is only
  ever written and never re-read for a given key would hold memory indefinitely, which would be a
  latent bug in a safety-critical context.
- Generic over `Key`/`Value` so the same class serves both the mandatory History cache
  (`key = {base, quote, startDate, endDate}`) and, optionally, a short-`max_age` cache for latest
  conversion rates (`key = {base, quote}`) — reused rather than reimplemented, in the spirit of
  Clean Code / DRY.

### 5.3 Thread safety

`grpc++`'s synchronous server dispatches concurrent RPCs onto a worker-thread pool, so the cache
**will** see concurrent access — this is called out explicitly in `task.md` ("thread-safety
considerations, in case requests arrive in parallel").

Decision: **a single `std::mutex` guarding the whole structure**, not a `std::shared_mutex`
(reader/writer lock). This looks like it leaves performance on the table, but it doesn't, for a
specific reason worth stating: every cache *read* in an LRU also mutates state (it moves the
accessed entry to the front of the list), so a "read" is never actually a read-only operation —
it would need the exclusive lock anyway. A `shared_mutex` here would add API surface and a false
sense of parallelism without a real benefit, which fails the Clean Code bar (no unjustified
complexity). This trade-off is documented in code as a comment, with **sharding by key hash**
named as the concrete next step if profiling ever showed contention — demonstrating awareness of
the scaling path without building it prematurely.

### 5.4 Why entries are dropped, not served stale

Once an entry exceeds `max_age` it is deleted outright, not marked "stale and returned with a
warning." This is a deliberate reading of the spec's wording ("must be automatically removed") over
the softer alternative (serve-stale-on-upstream-failure), because in a safety-critical mindset,
*serving data silently outside its declared validity window is the failure mode to avoid*, even if
it means a slower response (or a 503-equivalent `UNAVAILABLE` status, see §7) when the upstream API
is also down. This is a place where "clever" and "correct" pointed in different directions, and
`task.md`'s own framing was used as the tiebreaker.

### 5.5 Configuration

`max_age` (minutes) and `max_elements` load from a JSON file by default, overridable by CLI flags —
both requirements from `task.md` met simultaneously via `Poco::Util::JSONConfiguration` layered
under `Poco::Util::ServerApplication`'s option-handling framework (see §3): a `--config=<path>`
option loads the JSON file, and a repeatable `-D`/`--define KEY=VALUE` option (POCO's standard
Java-style "define" idiom) writes straight into the application's own highest-priority
configuration layer, so any `-D` override always wins over the file. Example:

```json
{
  "cache": {
    "history": { "max_age_minutes": 1440, "max_elements": 5000 },
    "latest_rate": { "max_age_minutes": 15, "max_elements": 500 }
  },
  "server": { "grpc_port": 9090 },
  "external_api": { "base_url": "https://api.frankfurter.dev", "timeout_ms": 5000 }
}
```

```
./currency-server --config=/etc/currency/server.json -Dcache.history.max_age_minutes=60
```

Per-cache values (rather than one global setting) so the two caches — history (rarely changes,
can live long) and latest-rate (changes daily) — are tuned independently, without over-engineering
a general plugin system that nothing in the task asks for.

## 6. Local Development & Deployment

Two separate, deliberately non-overlapping mechanisms cover "write the code" and "show the
result" — conflating them would slow down implementation for no benefit during development, and
would leave no single-command way to hand the finished project to someone else to run.

### 6.1 Nix Flake — Development

`flake.nix` provides a single reproducible `devShell` containing: the C++ toolchain (CMake,
compiler), `protobuf` + `grpc`, `poco`, Node.js/npm for Angular, and `grpcurl` for manual RPC
testing. This directly mitigates the trade-off named in §2 (gRPC-Web needs a third component): instead of
that being a "go install five things manually" burden for anyone trying the project,
`nix develop` (or `direnv`) reproduces the exact toolchain in one step. Envoy itself is
deliberately *not* one of these packages: nixpkgs only ships it as a source derivation on this
setup (no prebuilt binary substitute), and building it from source pulls in a multi-hour Bazel
build for a component that already has a well-maintained, officially published Docker image. Since
§6.2 runs Envoy exclusively via that image anyway, adding a from-source Envoy build to the devShell
would slow down every `nix develop` for a toolchain path nothing actually uses — the pragmatic
choice was to keep the shell fast and let Docker own Envoy end to end, not just in the final
Compose setup.
This is called out because a project with a protocol trade-off that's merely *described*
is weaker than one where the trade-off is *actively engineered around* — the flake is that
engineering. This is the environment used throughout implementation, where fast native
rebuild/reload cycles matter.

### 6.2 Docker Compose — Final Execution & Demo

For handing over or demonstrating the finished result, the same three processes from §1 are
packaged as three containers — `frontend` (Angular build served by `nginx`), `envoy` (the stock
`envoyproxy/envoy` image with `envoy/envoy.yaml` bind-mounted), and `backend` (the compiled
`currency-server`) — started together with `docker compose up --build`. This requires nothing on
the host beyond Docker itself, in contrast to the Nix devShell, which is still the right tool for
*working on* the code but requires whoever runs it to adopt Nix just to see it work.

The one design point worth calling out: the browser talks to `envoy` **directly** (per §1/§4 —
the frontend container never proxies gRPC traffic), so the frontend image cannot know the
backend's externally reachable URL at build time — that's a `docker-compose.yml` concern, decided
at `up` time, potentially different per environment. Baking it into the Angular production bundle
would force an image rebuild for every port/host change, which is the same class of mistake as
hardcoding business bounds into the client (§8) — configuration leaking into a compiled artifact.
The fix mirrors the backend's own config story (§5.5): a small startup script renders the backend
URL into a `env.js` file from an environment variable when the *container* starts, not when the
*image* is built, so one image works across environments.

The `backend` container additionally bind-mounts a host directory for its rotating log file, so
logs survive and stay readable independent of the container's lifecycle — see §9.3.

## 7. Error Handling

Mapped once, at the gRPC boundary (`grpc/CurrencyServiceImpl`), from domain exceptions to
`grpc::StatusCode`:

| Domain condition | `grpc::StatusCode` | Notes |
|---|---|---|
| Unknown/invalid currency code | `INVALID_ARGUMENT` | Validated against the cached currency list before any conversion/history logic runs |
| Amount outside configured min/max bounds | `INVALID_ARGUMENT` | Bounds are server-defined and exposed via `GetConstraints` (§ api.md) — never hardcoded on the client |
| Invalid date range (start > end, outside frankfurter.dev's supported range, future dates) | `INVALID_ARGUMENT` | Same reasoning — client only reflects server-declared bounds |
| frankfurter.dev unreachable / times out, no usable cache entry | `UNAVAILABLE` | Client shows a retryable error; never a silent empty result |
| frankfurter.dev responds with an unexpected schema | `INTERNAL` | Logged server-side with detail; client message stays generic — avoids leaking upstream internals, a small security-hygiene habit worth showing |
| Any uncaught `std::exception` | `INTERNAL` | A catch-all at the RPC boundary — a single bad request must never crash the process; this is the concrete, minimal expression of "safety-critical mindset" applied to a hobby-scale project |

The Angular client never inspects *why* an error occurred beyond the status code → it maps codes
to a user-facing message via a single interceptor and displays it (`MatSnackBar`). No branching
business logic on the client — consistent with the dumb-client principle applying to error
handling too, not just the happy path.

## 8. Frontend Architecture

- **Angular + Angular Material**, standalone components (no NgModules) — current idiomatic
  Angular, keeps the codebase small, which matters for a project meant to be read end-to-end.
- **One boundary service, `CurrencyApiService`**, wraps a `@connectrpc/connect` client built from
  the generated service descriptor, talking gRPC-Web via `@connectrpc/connect-web`'s
  `createGrpcWebTransport` (not the grpc-web project's own JS client — see `api.md §5` for why: its
  `protoc-gen-js` plugin isn't available through this repo's Nix toolchain, while `protoc-gen-es`
  is npm-installable and generates both messages and the service descriptor from one plugin; the
  wire protocol and Envoy setup are unchanged). No component talks to the generated stub directly.
  This mirrors the backend's "thin transport layer" idea on the client side: if the transport ever
  changed, one file changes.
- **State:** local component state via Angular signals. No NgRx/Akita. Justified directly by
  `task.md`'s "don't overengineer" note (aimed at charting, but the same restraint
  applies here) — the UI has two independent sections and no shared mutable state that would
  justify a store.
- **Charting: Chart.js via `ng2-charts`.** Preferred over `ngx-charts` because the only chart
  needed is a single time-series line chart; Chart.js is the smaller, more widely known
  dependency for that job, whereas `ngx-charts` pulls in D3 for capabilities (complex multi-chart
  compositions) this app doesn't use. Smaller dependency footprint for equivalent functionality is
  itself a Clean Code judgment call worth stating explicitly.
- **Bounds from the server, not hardcoded:** min/max amount and min/max date come from the
  `GetConstraints` RPC, fetched once at startup. This is the one place the "dumb client" principle
  is easiest to violate accidentally (it's tempting to just hardcode `min="1999-01-04"` in the
  template) — making it a server-declared value instead keeps the rule honest rather than
  nominal, and is called out explicitly in [`api.md`](api.md).

## 9. Logging & Observability

Not required by `task.md`, but valuable for demonstrating — to whoever runs the finished system,
not just whoever reads the code — that the caching and error-handling design from §5/§7 actually
behaves the way it's documented. Four things are logged: incoming requests, the cache
hit-vs-fetch decision, the outbound call to frankfurter.dev, and errors. All of it goes through
`Poco::Logger`, which is POCO used more fully rather than a fourth dependency added on top of the
choice already made in §3.

### 9.1 What is logged, where (mirrors the layering from §4)

| Layer | What is logged | Level |
|---|---|---|
| `grpc/CurrencyServiceImpl` | RPC name, summarized request, resulting status code, duration | INFORMATION |
| `application/*Service` | cache hit / miss decision, with domain context (currency pair, date range) | INFORMATION |
| `cache/TimeAndSizeBoundedCache` | generic put/evict events (capacity eviction, TTL sweep removals) — key only, no domain meaning | DEBUG |
| `external/FrankfurterClient` | outbound request URL, duration, HTTP status; failures (timeout, connection refused, unexpected schema) | INFORMATION / WARNING on failure |
| gRPC boundary (exception → `grpc::Status` mapping, §7) | full exception detail server-side; only the generic message crosses to the client | ERROR |

The cache class itself only ever logs mechanical, generic events (DEBUG) — it deliberately doesn't
know *why* a key was requested, which is exactly what makes it reusable (§5.2). The human-readable
"cache hit/miss for converting USD→EUR" statement is logged one layer up, in the application
service that has that domain context. This is the same layering discipline from §4 applied to
logging, rather than treating logging as a cross-cutting concern exempt from it.

### 9.2 Enable/disable via configuration

A single `logging.level` key (`off | error | warning | information | debug`), loaded and
overridable through the *same* `Poco::Util::JSONConfiguration` + CLI-argument mechanism already
used for `cache.*` (§5.5) — not a separate `logging.enabled` boolean next to a level, because two
knobs that can contradict each other (`enabled: false, level: debug`) is exactly the kind of
inconsistent-state bug a safety-critical mindset avoids by construction rather than convention.
`off` maps to `Poco::Message::PRIO_NONE` on the root logger — no output, negligible overhead.

```json
{
  "logging": { "level": "information", "file": "/var/log/currency-server/backend.log" }
}
```

```
./currency-server --config=/app/config/server.default.json -Dlogging.level=debug
```

Call sites use POCO's `poco_information(...)`/`poco_debug(...)` macros rather than hand-written
`if (loggingEnabled) log(...)` guards scattered through business logic — the macros check the
configured level before evaluating the message, so turning logging off is a one-line config
change with no second, redundant conditional living next to every log call. The code reads
identically whether logging ends up on or off.

### 9.3 Sharing logs with the host (Docker)

Every log statement is fed through a `Poco::SplitterChannel` into two sinks at once:

- a `Poco::ConsoleChannel` (stdout) — picked up automatically by Docker's log driver, so
  `docker compose logs -f backend` works with no extra setup;
- a `Poco::FileChannel` (POCO's own size/time-based rotation and archiving, so no extra
  `logrotate` setup is needed) writing to `/var/log/currency-server/backend.log` **inside** the
  container.

`docker-compose.yml` bind-mounts a host directory over that file path (`./logs/backend` →
`/var/log/currency-server`, so the log file is also
readable/tailable directly on the host, independent of whether a terminal is still attached to the
container. Both sinks are kept rather than picking one, because they serve different moments:
`docker compose logs` for watching the system live during a demo, the mounted file for looking
back afterwards or grepping without an attached terminal.

## 10. Summary of Key Decisions

| Area | Decision | Primary reason (application context) |
|---|---|---|
| Client↔Server protocol | gRPC (+ gRPC-Web + Envoy) | Direct match to `task.md`'s protocol requirements; contract-first safety |
| Backend↔frankfurter.dev | POCO (`Net` + `JSON` + `Util`) | Solves config-loading requirement for free; keeps effort on the cache |
| Cache structure | `unordered_map` + `list` (LRU) + lazy/periodic TTL sweep | Textbook, minimal, satisfies both mandated eviction strategies |
| Cache locking | Single `std::mutex` | LRU touch-on-read needs exclusive access anyway; avoids false complexity |
| Layering | Transport / Application / Infrastructure(cache, external client) / Domain | Matches `task.md`'s explicit layering ask; enables DI |
| Frontend state | Local signals, no store | Nothing in the app needs cross-component shared state |
| Charting | Chart.js (`ng2-charts`) | Smallest dependency for a single line chart; avoids D3 weight |
| Dev environment | Nix flake `devShell` | Mitigates the extra-infra cost of choosing gRPC-Web |
| Final execution / demo | Docker Compose, 3 containers (frontend/envoy/backend) | Single-command, toolchain-free way to run and show the finished system |
| Logging | `Poco::Logger`, configurable `logging.level`, console + rotating file, file bind-mounted to host | Makes cache/error behavior observable while running; reuses the POCO decision from §3 |
