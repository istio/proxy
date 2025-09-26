#include "http_client.h"

#include <algorithm>

#include "string_util.h"

namespace datadog {
namespace tracing {

constexpr StringView k_scheme_separator = "://";
constexpr StringView k_supported[] = {"http", "https", "unix", "http+unix",
                                      "https+unix"};

Expected<HTTPClient::URL> HTTPClient::URL::parse(StringView input) {
  const auto after_scheme = input.find(k_scheme_separator);
  if (after_scheme == StringView::npos) {
    std::string message;
    message += "Datadog Agent URL is missing the \"://\" separator: \"";
    append(message, input);
    message += '\"';
    return Error{Error::URL_MISSING_SEPARATOR, std::move(message)};
  }

  const StringView scheme = input.substr(0, after_scheme);
  const auto found =
      std::find(std::begin(k_supported), std::end(k_supported), scheme);
  if (found == std::end(k_supported)) {
    std::string message;
    message += "Unsupported URI scheme \"";
    append(message, scheme);
    message += "\" in Datadog Agent URL \"";
    append(message, input);
    message += "\". The following are supported:";
    for (const auto& supported_scheme : k_supported) {
      message += ' ';
      append(message, supported_scheme);
    }
    return Error{Error::URL_UNSUPPORTED_SCHEME, std::move(message)};
  }

  const StringView authority_and_path =
      input.substr(after_scheme + k_scheme_separator.size());
  // If the scheme is for unix domain sockets, then there's no way to
  // distinguish the path-to-socket from the path-to-resource.  Some
  // implementations require that the forward slashes in the path-to-socket
  // are URL-encoded.  However, URLs that we will be parsing designate the
  // location of the Datadog Agent service, and so do not have a resource
  // location.  Thus, if the scheme is for a unix domain socket, assume that
  // the entire part after the "://" is the path to the socket, and that
  // there is no resource path.
  if (scheme == "unix" || scheme == "http+unix" || scheme == "https+unix") {
    if (authority_and_path.empty() || authority_and_path[0] != '/') {
      std::string message;
      message +=
          "Unix domain socket paths for Datadog Agent must be absolute, i.e. "
          "must begin with a "
          "\"/\". The path \"";
      append(message, authority_and_path);
      message += "\" is not absolute. Error occurred for URL: \"";
      append(message, input);
      message += '\"';
      return Error{Error::URL_UNIX_DOMAIN_SOCKET_PATH_NOT_ABSOLUTE,
                   std::move(message)};
    }
    return HTTPClient::URL{std::string(scheme), std::string(authority_and_path),
                           ""};
  }

  // The scheme is either "http" or "https".  This means that the part after
  // the "://" could be <resource>/<path>, e.g. "localhost:8080/api/v1".
  // Again, though, we're only parsing URLs that designate the location of
  // the Datadog Agent service, and so they will not have a resource
  // location.  Still, let's parse it properly.
  const auto after_authority = authority_and_path.find('/');
  return HTTPClient::URL{
      std::string(scheme),
      std::string(authority_and_path.substr(0, after_authority)),
      (after_authority == StringView::npos)
          ? ""
          : std::string(authority_and_path.substr(after_authority))};
}

}  // namespace tracing
}  // namespace datadog
