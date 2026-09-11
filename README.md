# Currency Converter

A client-server application for browsing currencies, converting amounts, and visualizing
historical exchange rate trends, built against
[https://api.frankfurter.dev](https://api.frankfurter.dev).

- **Backend** (`backend/`): C++ / POCO / gRPC server. Talks to frankfurter.dev, caches results
  (time- and size-bounded), and exposes `currency.CurrencyService` over gRPC.
- **Frontend** (`frontend/`): Angular / Angular Material "dumb client" -- all business logic and
  validation live on the backend.
- **Envoy** (`envoy/`): translates gRPC-Web (browser) to native gRPC (backend).

Full design rationale: [`docs/architecture.md`](docs/architecture.md) ·
API contract: [`docs/api.md`](docs/api.md)

## Run it

### Option A — Docker Compose (packaged, no local toolchain beyond Docker)

```sh
docker compose up --build
```

Then open **http://localhost:4200**.

### Option B — Nix devShell (for reading/modifying the code)

Requires [Nix](https://nixos.org) with flakes enabled.

```sh
nix develop   # or: direnv allow
```

Three processes, three terminals, from the repo root:

```sh
# 1. Backend
cd backend
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/currency_server

# 2. Envoy
docker run --rm --network host \
  --add-host=backend:127.0.0.1 \
  -v "$PWD/envoy/envoy.yaml:/etc/envoy/envoy.yaml:ro" \
  envoyproxy/envoy:v1.31-latest

# 3. Frontend
cd frontend
npm run generate:proto   # regenerate TS stubs from proto/currency.proto
npm start
```

Then open **http://localhost:4200**.

## Configuration

The backend reads `backend/config/server.default.json` by default; both `max_age`/`max_elements`
per cache and the logging level can be overridden without touching the file:

```sh
./currency_server --config=/path/to/config.json -Dcache.history.max_age_minutes=60 -Dlogging.level=debug
```

See [`docs/architecture.md` §5.5](docs/architecture.md#55-configuration) and
[§9.2](docs/architecture.md#92-enabledisable-via-configuration).

## Logs

The backend logs requests, cache hit/miss decisions, calls to frankfurter.dev, and errors. Under
Docker Compose, they're bind-mounted to `./logs/backend/backend.log` on the host as well as
available via `docker compose logs -f backend` -- see
[`docs/architecture.md` §9](docs/architecture.md#9-logging--observability).