# Multi-stage build using Debian's packaged protobuf/gRPC/POCO (apt) rather
# than the exact Nix-pinned versions from flake.nix. A Nix-built binary is
# dynamically linked against /nix/store paths that don't exist in a plain
# runtime image -- reproducing it here would mean either shipping the whole
# Nix closure into the image or building the final image with Nix's own
# dockerTools, both meaningfully more moving parts than a conventional
# apt-based build for a project this size. Nix (flake.nix) stays the tool
# for local-dev reproducibility; Docker uses the ecosystem's own standard
# packaging for the shipped artifact instead of trying to unify the two.
#
# Build context must be the repository root (not backend/), since this also
# needs ../proto/currency.proto -- docker-compose.yml sets that up.

FROM debian:bookworm-slim AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential \
      cmake \
      ninja-build \
      pkg-config \
      protobuf-compiler \
      protobuf-compiler-grpc \
      libprotobuf-dev \
      libgrpc++-dev \
      libssl-dev \
      libpoco-dev \
      libfmt-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
COPY proto/ ./proto/
COPY backend/ ./backend/

WORKDIR /workspace/backend
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j"$(nproc)"

# ---------------------------------------------------------------------------

FROM debian:bookworm-slim AS runtime

# Same -dev packages as the build stage rather than hand-picked runtime-only
# library package names: apt's own dependency resolution pulls in the
# matching .so for us, at the cost of a somewhat larger image than a
# minimal runtime-only install -- a deliberate trade-off against guessing
# exact versioned runtime package names blind.
RUN apt-get update && apt-get install -y --no-install-recommends \
      libprotobuf-dev \
      libgrpc++-dev \
      libssl-dev \
      libpoco-dev \
      libfmt-dev \
      ca-certificates \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --no-create-home --shell /usr/sbin/nologin currency

WORKDIR /app
COPY --from=build /workspace/backend/build/currency_server /app/currency_server
COPY backend/config/server.default.json /app/config/server.default.json

# /var/log/currency-server is where docker-compose.yml bind-mounts a host
# directory so logs are readable outside the container too.
RUN mkdir -p /var/log/currency-server && chown currency:currency /var/log/currency-server
USER currency

EXPOSE 9090
ENTRYPOINT ["/app/currency_server", "--config=/app/config/server.default.json"]
