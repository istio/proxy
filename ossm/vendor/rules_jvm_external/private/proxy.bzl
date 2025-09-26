def _strip_url_scheme(url):
    return url.split("://", 1)[1] if "://" in url else url

def _get_proxy_user(url):
    no_scheme_url = _strip_url_scheme(url)
    if "@" in no_scheme_url:
        userinfo = no_scheme_url.rsplit("@", 1)[0]
        if ":" in userinfo:
            userinfo = userinfo.split(":", 1)[0]
        return userinfo
    return None

def _get_proxy_password(url):
    no_scheme_url = _strip_url_scheme(url)
    if "@" in no_scheme_url:
        userinfo = no_scheme_url.rsplit("@", 1)[0]
        if ":" in userinfo:
            userinfo = userinfo.split(":", 1)[1]
        return userinfo
    return None

def _get_proxy_hostname(url):
    no_scheme_url = _strip_url_scheme(url)
    netloc = no_scheme_url.split("@")[-1]
    if ":" in netloc:
        return netloc.split(":")[0]
    else:
        return netloc

def _get_proxy_port(url):
    no_scheme_url = _strip_url_scheme(url)
    netloc = no_scheme_url.split("/")[0].split("@")[-1]
    if ":" in netloc:
        return netloc.split(":")[1]
    else:
        return None

# Extract the well-known environment variables http_proxy, https_proxy and
# no_proxy and convert them to java.net-compatible property arguments.
def get_java_proxy_args(http_proxy, https_proxy, no_proxy):
    proxy_args = []

    if http_proxy:
        # We only need to set proxyProtocol until https://github.com/coursier/coursier/pull/2701
        # is merged and we can update coursier
        proxy_args.append("-Dhttp.proxyProtocol=http")
        proxy_user = _get_proxy_user(http_proxy)
        if proxy_user:
            proxy_args.append("-Dhttp.proxyUser=%s" % proxy_user)
        proxy_password = _get_proxy_password(http_proxy)
        if proxy_password:
            proxy_args.append("-Dhttp.proxyPassword=%s" % proxy_password)
        proxy_host = _get_proxy_hostname(http_proxy)
        if proxy_host:
            proxy_args.append("-Dhttp.proxyHost=%s" % proxy_host)
        proxy_port = _get_proxy_port(http_proxy)
        if proxy_port:
            proxy_args.append("-Dhttp.proxyPort=%s" % proxy_port)

    if https_proxy:
        # We only need to set proxyProtocol until https://github.com/coursier/coursier/pull/2701
        # is merged and we can update coursier
        proxy_args.append("-Dhttps.proxyProtocol=https")
        proxy_user = _get_proxy_user(https_proxy)
        if proxy_user:
            proxy_args.append("-Dhttps.proxyUser=%s" % proxy_user)
        proxy_password = _get_proxy_password(https_proxy)
        if proxy_password:
            proxy_args.append("-Dhttps.proxyPassword=%s" % proxy_password)
        proxy_host = _get_proxy_hostname(https_proxy)
        if proxy_host:
            proxy_args.append("-Dhttps.proxyHost=%s" % proxy_host)
        proxy_port = _get_proxy_port(https_proxy)
        if proxy_port:
            proxy_args.append("-Dhttps.proxyPort=%s" % proxy_port)

    # Convert no_proxy-style exclusions, including base domain matching, into java.net nonProxyHosts:
    # localhost,example.com,foo.example.com,.otherexample.com -> "localhost|example.com|foo.example.com|*.otherexample.com"
    if no_proxy:
        if no_proxy.startswith("."):
            no_proxy = "*" + no_proxy
        proxy_args.append("-Dhttp.nonProxyHosts=%s" % no_proxy.replace(",", "|").replace("|.", "|*."))

    return proxy_args
