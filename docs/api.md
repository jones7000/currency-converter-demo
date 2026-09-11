# API Contract

Single source of truth: [`proto/currency.proto`](../proto/currency.proto).
Both the C++ backend and the Angular frontend generate their stubs from this
one file — see [`architecture.md §2`](architecture.md#2-decision-protocol--grpc-over-rest) for why
gRPC was chosen over REST despite the task allowing either.

The template provided in `task.md` defined **messages** but no **service** (no RPC methods) — the
task explicitly says "you are free to extend or modify" it. The extensions below are the minimum
needed to cover both required UI sections plus the "dumb client" principle (bounds must come from
the server, not be hardcoded in Angular).

## 1. Full `.proto`

```protobuf
syntax = "proto3";
package currency;

// ── Service ────────────────────────────────────────────────────────────────

service CurrencyService {
  // Part A: Currency Overview — full list of supported currencies.
  rpc ListCurrencies(ListCurrenciesRequest) returns (CurrencyListResponse);

  // Part B: convert a fixed amount at the latest available rate.
  rpc Convert(ConvertRequest) returns (ConvertResponse);

  // Part B: historical rate series between two currencies over a date range.
  rpc GetHistory(HistoryRequest) returns (HistoryResponse);

  // UI bounds (min/max amount, min/max selectable date) — server-declared,
  // so the client never hardcodes business rules. See architecture.md §8.
  rpc GetConstraints(ConstraintsRequest) returns (ConstraintsResponse);
}

// ── Part A: Currency Overview ────────────────────────────────────────────

message ListCurrenciesRequest {}

message CurrencyInfo {
  string code = 1;       // ISO 4217, e.g. "USD"
  string full_name = 2;  // e.g. "United States Dollar"
}

message CurrencyListResponse {
  repeated CurrencyInfo currencies = 1;
}

// ── Part B: Conversion ───────────────────────────────────────────────────

message ConvertRequest {
  string source_currency = 1;  // ISO 4217 code
  string target_currency = 2;  // ISO 4217 code
  double amount = 3;           // must satisfy [min_amount, max_amount] from ConstraintsResponse
}

message ConvertResponse {
  double source_amount = 1;
  double converted_amount = 2;
  double exchange_rate = 3;
  string rate_date = 4;  // ISO 8601 date (YYYY-MM-DD) of the latest rate used
}

// ── Part B: Historical Trend ─────────────────────────────────────────────

message HistoryRequest {
  string source_currency = 1;
  string target_currency = 2;
  string start_date = 3;  // ISO 8601 date, inclusive
  string end_date = 4;    // ISO 8601 date, inclusive
}

message HistoryPoint {
  string date = 1;
  double rate = 2;
}

message HistoryResponse {
  string source_currency = 1;
  string target_currency = 2;
  repeated HistoryPoint points = 3;
}

// ── UI Bounds ────────────────────────────────────────────────────────────

message ConstraintsRequest {}

message ConstraintsResponse {
  double min_amount = 1;
  double max_amount = 2;
  string min_date = 3;  // earliest date frankfurter.dev has data for
  string max_date = 4;  // latest date with published rates (today or last business day)
}
```

Everything from the task's original template is preserved byte-for-byte (field names, numbers,
types) — only additive changes were made (wrapper messages for list responses, the `service`
block, and the constraints RPC), so nothing the task specified was altered.

## 2. RPC Semantics

### `ListCurrencies`
- Backend source: frankfurter.dev `GET /v1/currencies`.
- Caching: cached like everything else the backend fetches (short/medium `max_age`, e.g. hours —
  this list changes essentially never), reusing the generic cache from `architecture.md §5`, even
  though `task.md`'s mandatory-caching clause names historical data specifically. Caching it too is
  a small, justified extension: it is the same external dependency and the same reasoning applies,
  at negligible extra cost.

### `Convert`
- Always uses the **latest** available rate (frankfurter.dev `GET /v1/latest?...`) — the task's UI
  spec asks for "the converted amount, alongside the date of the latest rate used," not a
  user-selectable conversion date. No `date` field was added to `ConvertRequest` for this reason —
  adding one would be speculative scope the task didn't ask for.
- Validation, in order, each returning `INVALID_ARGUMENT` on failure (see `architecture.md §7`):
  1. `source_currency` and `target_currency` are known ISO codes (checked against the currency list).
  2. `source_currency != target_currency` — rejected rather than silently returning rate `1.0`,
     because a no-op conversion request most likely indicates a client-side selection bug and
     should surface, not be masked.
  3. `amount` is finite, positive, and within `[min_amount, max_amount]` from `GetConstraints`.

### `GetHistory`
- Backend source: frankfurter.dev `GET /v1/{start_date}..{end_date}?...`.
- Cache key: `(source_currency, target_currency, start_date, end_date)` — the exact query tuple,
  matching `task.md`'s wording ("repeated query for data that is already cached"). No partial-range
  cache merging (e.g. reusing a cached `[2024-01-01, 2024-06-01]` to answer a `[2024-02-01,
  2024-03-01]` request), since it adds real complexity (interval overlap logic) for a benefit
  the task doesn't require.
- Validation, each `INVALID_ARGUMENT`:
  1. Both currency codes known.
  2. `start_date <= end_date`.
  3. Both dates within `[min_date, max_date]` from `GetConstraints`.

### `GetConstraints`
- `min_date` / `max_date`: derived from frankfurter.dev's own supported range (its earliest data is
  1999-01-04; `max_date` is "today" adjusted for the fact that frankfurter.dev doesn't publish
  same-day rates until they're available — the backend resolves this once and caches it, rather
  than the client guessing).
- `min_amount` / `max_amount`: sensible application-defined bounds (e.g. `0.01` and
  `1_000_000_000`) preventing degenerate inputs (zero, negative, absurdly large) from ever reaching
  the external API. These are business rules by definition, which is exactly why they live on the
  backend and are only *displayed* by the client — the concrete, checkable instance of the "dumb
  client" requirement.
- This RPC is called once at Angular app startup and its result held in a single shared service;
  it is not re-fetched per form interaction.

## 3. Error Model

gRPC status codes are the entire error contract — no custom error-message parsing is required on
the client. Full mapping table lives in `architecture.md §7`; summarized here for the client's
perspective:

| Status | Client behavior |
|---|---|
| `INVALID_ARGUMENT` | Show the status message inline near the offending field (amount/date/currency) |
| `UNAVAILABLE` | Show a retryable "service unavailable" banner (`MatSnackBar`, action: retry) |
| `NOT_FOUND` | Treated as `INVALID_ARGUMENT` for display purposes (unknown currency) |
| `INTERNAL` | Generic "something went wrong" message; full detail only in backend logs |

The client performs this mapping in exactly one place (a gRPC-Web interceptor / the
`CurrencyApiService` boundary — `architecture.md §8`), never per-component.

## 4. Transport Details (gRPC-Web / Envoy)

- Angular calls the backend via `@connectrpc/connect-web`'s `createGrpcWebTransport`, which speaks
  the same gRPC-Web wire protocol the grpc-web project's own client would (binary framing,
  `application/grpc-web+proto`) — Envoy and the backend are unaware of which client library
  produced it. See §5 for why the codegen tool differs from the originally-planned
  `protoc-gen-grpc-web`.
- Envoy listens on the port Angular calls (e.g. `:8080`), terminates gRPC-Web framing and CORS, and
  forwards native gRPC (HTTP/2) to the backend's gRPC port (e.g. `:9090`). Config lives at
  `envoy/envoy.yaml`.
- The backend has no gRPC-Web awareness at all — it only ever speaks native gRPC. This keeps the
  translation concern isolated to the proxy, consistent with the layering principle applied at the
  infrastructure level, not just inside the C++ process.

## 5. Codegen — Keeping the Contract Single-Sourced

- `proto/currency.proto` lives at the repository root (outside both `backend/` and `frontend/`) so
  neither side "owns" it.
- Backend: generated via CMake (`protobuf_generate_cpp` / the gRPC CMake helpers) as part of the
  normal build — generated code is never committed.
- Frontend: generated via an `npm run generate:proto` script wrapping `protoc` +
  `@bufbuild/protoc-gen-es` (`target=ts`), output to `frontend/src/app/generated/` (git-ignored).
  **Deviation from the original plan:** the grpc-web project's own `protoc-gen-js` plugin isn't
  packaged in the nixpkgs snapshot this repo's `flake.nix` pins, and installing it would mean a
  platform-specific binary download outside Nix's reproducibility story. `protoc-gen-es` is
  npm-installable (so it lives in `package.json` like any other dependency, no extra system
  package needed) and, since Connect-ES v2, generates both message types *and* the service
  descriptor from a single plugin — `@connectrpc/connect`'s generic `createClient()` consumes that
  descriptor directly, so no second `protoc-gen-connect-es` plugin is needed either. Only the
  client-side codegen tool changed; the wire protocol (gRPC-Web, via Envoy) and the `.proto`
  contract itself are exactly as designed.
- Any change to the `.proto` file requires regenerating both sides before the build succeeds —
  this is the concrete mechanism behind the "reduces drift between client and server models" claim
  in `architecture.md §2`: drift isn't just discouraged, it's a build failure.
