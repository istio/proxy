FROM jaegertracing/all-in-one:1.71.0@sha256:beb31282a9c5d0d10cb78dd168945dab9887acebb42fcc0bd738b08c36b68bc0
HEALTHCHECK \
    --interval=1s \
    --timeout=1s \
    --start-period=1s \
    --retries=60 \
    CMD wget -q --header='Content-Type:application/json' -O - http://localhost:14269/health | grep "Server available"
