FROM debian:buster
ADD extensionserver /extensionserver
RUN chmod a+x /extensionserver
EXPOSE 8080
CMD ["/extensionserver", "-c", "/etc/extensionserver/config"]
