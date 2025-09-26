FROM rust:1.88.0 AS builder
WORKDIR /usr/src/echo
COPY . .
RUN rustup install nightly && rustup default nightly && cargo build --release


FROM debian:bookworm-slim
RUN --mount=type=tmpfs,target=/var/cache/apt \
    --mount=type=tmpfs,target=/var/lib/apt/lists \
    apt-get -qq update \
    && apt-get -qq install -y libssl-dev ca-certificates
COPY --from=builder \
    /usr/src/echo/target/release/example-echo \
    /usr/local/bin/example-echo
EXPOSE 8080
COPY <<EOF /etc/config.yaml
listeners:
  http:
    port: 8080
    host: >-
      0.0.0.0

EOF
COPY ./snakeoil /tmp/snakeoil
CMD ["example-echo", "-c", "/etc/config.yaml"]
