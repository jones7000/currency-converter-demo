// Local-dev default (served as a static asset under `ng serve`/`ng build`).
// The Docker image overwrites this file from $BACKEND_URL at container
// start -- see docker/entrypoint.sh
window.__env = {
  backendUrl: 'http://localhost:8080',
};
