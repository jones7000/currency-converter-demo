#!/bin/sh
set -eu

# Runs as an nginx /docker-entrypoint.d/ hook (executed automatically by the
# official nginx image before nginx starts). Renders env.template.js into
# the served env.js using the *container's* $BACKEND_URL, so one built image
# works against whatever host/port docker-compose.yml maps Envoy to --
# baking the URL in at `npm run build` time would force an image rebuild
# for every port/host change.
: "${BACKEND_URL:=http://localhost:8080}"

envsubst '${BACKEND_URL}' \
  < /usr/share/nginx/html/env.template.js \
  > /usr/share/nginx/html/env.js

echo "entrypoint: BACKEND_URL=${BACKEND_URL} -> env.js rendered"
