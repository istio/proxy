load("@bazel_skylib//lib:unittest.bzl", "asserts", "unittest")
load("//private:proxy.bzl", "get_java_proxy_args")

def _java_proxy_parsing_empty_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(env, [], get_java_proxy_args("", "", ""))
    asserts.equals(env, [], get_java_proxy_args(None, None, None))
    return unittest.end(env)

java_proxy_parsing_empty_test = unittest.make(_java_proxy_parsing_empty_test_impl)

def _java_proxy_parsing_no_scheme_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        [
            "-Dhttp.proxyProtocol=http",
            "-Dhttp.proxyHost=localhost",
            "-Dhttp.proxyPort=8888",
            "-Dhttps.proxyProtocol=https",
            "-Dhttps.proxyHost=localhost",
            "-Dhttps.proxyPort=8843",
            "-Dhttp.nonProxyHosts=google.com",
        ],
        get_java_proxy_args("localhost:8888", "localhost:8843", "google.com"),
    )
    return unittest.end(env)

java_proxy_parsing_no_scheme_test = unittest.make(_java_proxy_parsing_no_scheme_test_impl)

def _java_proxy_parsing_no_user_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        [
            "-Dhttp.proxyProtocol=http",
            "-Dhttp.proxyHost=example.com",
            "-Dhttp.proxyPort=80",
            "-Dhttps.proxyProtocol=https",
            "-Dhttps.proxyHost=secureexample.com",
            "-Dhttps.proxyPort=443",
            "-Dhttp.nonProxyHosts=google.com",
        ],
        get_java_proxy_args("http://example.com:80", "https://secureexample.com:443", "google.com"),
    )
    return unittest.end(env)

java_proxy_parsing_no_user_test = unittest.make(_java_proxy_parsing_no_user_test_impl)

def _java_proxy_parsing_no_port_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        [
            "-Dhttp.proxyProtocol=http",
            "-Dhttp.proxyHost=example.com",
            "-Dhttps.proxyProtocol=https",
            "-Dhttps.proxyHost=secureexample.com",
            "-Dhttp.nonProxyHosts=google.com",
        ],
        get_java_proxy_args("http://example.com", "https://secureexample.com", "google.com"),
    )
    return unittest.end(env)

java_proxy_parsing_no_port_test = unittest.make(_java_proxy_parsing_no_port_test_impl)

def _java_proxy_parsing_trailing_slash_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        [
            "-Dhttp.proxyProtocol=http",
            "-Dhttp.proxyHost=example.com",
            "-Dhttp.proxyPort=80",
            "-Dhttps.proxyProtocol=https",
            "-Dhttps.proxyHost=secureexample.com",
            "-Dhttps.proxyPort=443",
            "-Dhttp.nonProxyHosts=google.com",
        ],
        get_java_proxy_args("http://example.com:80", "https://secureexample.com:443/", "google.com"),
    )
    return unittest.end(env)

java_proxy_parsing_trailing_slash_test = unittest.make(_java_proxy_parsing_trailing_slash_test_impl)

def _java_proxy_parsing_all_test_impl(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        [
            "-Dhttp.proxyProtocol=http",
            "-Dhttp.proxyUser=user1",
            "-Dhttp.proxyPassword=pass1",
            "-Dhttp.proxyHost=example.com",
            "-Dhttp.proxyPort=80",
            "-Dhttps.proxyProtocol=https",
            "-Dhttps.proxyUser=user2",
            "-Dhttps.proxyPassword=pass2",
            "-Dhttps.proxyHost=secureexample.com",
            "-Dhttps.proxyPort=443",
            "-Dhttp.nonProxyHosts=google.com|localhost",
        ],
        get_java_proxy_args("http://user1:pass1@example.com:80", "https://user2:pass2@secureexample.com:443", "google.com,localhost"),
    )
    return unittest.end(env)

java_proxy_parsing_all_test = unittest.make(_java_proxy_parsing_all_test_impl)

def proxy_test_suite():
    unittest.suite(
        "proxy_tests",
        java_proxy_parsing_empty_test,
        java_proxy_parsing_no_scheme_test,
        java_proxy_parsing_no_user_test,
        java_proxy_parsing_no_port_test,
        java_proxy_parsing_trailing_slash_test,
        java_proxy_parsing_all_test,
    )
