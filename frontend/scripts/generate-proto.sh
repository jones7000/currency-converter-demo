#!/usr/bin/env bash
set -euo pipefail

# Regenerates the TypeScript client stubs from proto/currency.proto -- the
# single source of truth shared with the backend. Generated output is
# git-ignored; this script is the only thing that produces it, so
# client/server drift becomes "forgot to regenerate" at worst, never silent
# divergence.
#
# Codegen: @bufbuild/protoc-gen-es (v2) generates both message types *and*
# the service descriptor from one plugin -- @connectrpc/connect's generic
# createClient() consumes that descriptor directly, so no second
# "protoc-gen-connect-es" plugin is needed (that was Connect-ES v1's split;
# v2 unified it). The wire protocol is still plain gRPC-Web, spoken by
# @connectrpc/connect-web's createGrpcWebTransport against the same Envoy
# proxy -- only the client-side codegen tool changed (nixpkgs doesn't
# currently package the older grpc-web project's `protoc-gen-js` plugin,
# while protoc-gen-es installs cleanly from npm).
#
# Requires `protoc` on PATH (from the repo's Nix devShell) and
# node_modules/.bin/protoc-gen-es (from `npm install`).

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FRONTEND_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REPO_ROOT="$(cd "$FRONTEND_DIR/.." && pwd)"
OUT_DIR="$FRONTEND_DIR/src/app/generated"
PROTOC_GEN_ES="$FRONTEND_DIR/node_modules/.bin/protoc-gen-es"

command -v protoc >/dev/null || { echo "protoc not found -- run this inside 'nix develop'" >&2; exit 1; }
[ -x "$PROTOC_GEN_ES" ] || { echo "protoc-gen-es not found -- run 'npm install' in frontend/ first" >&2; exit 1; }

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

protoc \
  --proto_path="$REPO_ROOT/proto" \
  --plugin="protoc-gen-es=$PROTOC_GEN_ES" \
  --es_out="$OUT_DIR" \
  --es_opt=target=ts \
  "$REPO_ROOT/proto/currency.proto"

echo "Generated TypeScript stubs in $OUT_DIR"
