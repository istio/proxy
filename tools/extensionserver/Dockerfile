FROM debian:buster
RUN apt-get update \
 && apt-get install -y --no-install-recommends ca-certificates
RUN update-ca-certificates
ADD extensionserver /extensionserver
RUN chmod a+x /extensionserver
EXPOSE 8080
CMD ["/extensionserver", "-c", "/etc/extensionserver/config"]
