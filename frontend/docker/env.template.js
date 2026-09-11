// Rendered to /usr/share/nginx/html/env.js at *container start* (not image
// build time) by docker/entrypoint.sh, substituting $BACKEND_URL. See
// src/app/core/env.ts and docs/architecture.md §6.2 for why this is
// runtime, not build-time, configuration.
window.__env = {
  backendUrl: '${BACKEND_URL}',
};
