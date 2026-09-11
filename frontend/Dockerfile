# Multi-stage build. Build context must be the repository root (see
# docker-compose.yml), since this also needs ../proto/currency.proto.

FROM node:22-bookworm-slim AS build

# protoc regenerates the TypeScript client stubs from currency.proto as part
# of the image build -- generated code is never committed, so the Docker
# build reproduces exactly what `npm run generate:proto` does locally,
# using the same script.
RUN apt-get update && apt-get install -y --no-install-recommends protobuf-compiler \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
COPY proto/ ./proto/
COPY frontend/ ./frontend/

WORKDIR /workspace/frontend
RUN npm ci
RUN npm run generate:proto
RUN npx ng build --configuration production

# ---------------------------------------------------------------------------

FROM nginx:1.27-alpine AS runtime

COPY frontend/docker/nginx.conf /etc/nginx/conf.d/default.conf
COPY frontend/docker/env.template.js /usr/share/nginx/html/env.template.js
COPY frontend/docker/entrypoint.sh /docker-entrypoint.d/50-render-env.sh
RUN chmod +x /docker-entrypoint.d/50-render-env.sh

COPY --from=build /workspace/frontend/dist/frontend/browser /usr/share/nginx/html

EXPOSE 80
