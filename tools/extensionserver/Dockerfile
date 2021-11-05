FROM debian:buster
RUN apt-get update \
 && apt-get install -y --no-install-recommends ca-certificates \
 && rm -rf /var/lib/apt/lists/*
RUN update-ca-certificates
COPY extensionserver /extensionserver
RUN chmod a+x /extensionserver
EXPOSE 8080
CMD ["/extensionserver", "-c", "/etc/extensionserver/config"]
