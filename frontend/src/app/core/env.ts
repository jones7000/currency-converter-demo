// Runtime (not build-time) configuration. The browser calls Envoy's
// host-mapped port directly, which is only known once the container
// actually starts -- baking it into the production bundle would force an
// image rebuild for every port/host change. docker/entrypoint.sh renders
// this value into public/env.js from $BACKEND_URL at container start;
// index.html loads that script before the Angular bundle.
export interface RuntimeEnv {
  backendUrl: string;
}

declare global {
  interface Window {
    __env?: RuntimeEnv;
  }
}

// Falls back to the local-dev default (Envoy's port) when env.js wasn't
// loaded, e.g. under `ng serve`.
export function resolveBackendUrl(): string {
  return window.__env?.backendUrl || 'http://localhost:8080';
}
