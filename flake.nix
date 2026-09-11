{
  description = "Currency Converter - dev environment (C++/gRPC backend, Angular/gRPC-Web frontend, Envoy proxy)";

  inputs = {
    nixpkgs.url = "nixpkgs";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
      in
      {
        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            # Backend toolchain
            cmake
            ninja
            gcc
            pkg-config
            protobuf
            grpc
            poco
            fmt

            # Frontend toolchain
            # (TS codegen itself uses @bufbuild/protoc-gen-es, an npm
            # devDependency invoked via node_modules/.bin -- see
            # frontend/scripts/generate-proto.sh for why this isn't the
            # grpc-web project's own protoc-gen-js/-grpc-web plugins.)
            nodejs_22

            # Convenience tooling
            grpcurl
          ];

          shellHook = ''
            echo "Currency Converter dev shell"
            echo "  protoc:            $(protoc --version)"
            echo "  cmake:             $(cmake --version | head -1)"
            echo "  grpc_cpp_plugin:   $(command -v grpc_cpp_plugin)"
            echo "  poco:              ${pkgs.poco.version}"
            echo "  node:              $(node --version)"
            echo "  envoy:              run via the official envoyproxy/envoy Docker image (see envoy/, docker-compose.yml) -- not built through nix, to keep this shell fast"
          '';
        };
      });
}
