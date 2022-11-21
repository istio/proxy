load("@bazel_skylib//lib:dicts.bzl", "dicts")

ENVOY_{{ .EnvoyExtensions }}

ENVOY_CONTRIB_EXTENSIONS = {
    #
    # HTTP filters
    #

    "envoy.filters.http.dynamo":                                "//contrib/dynamo/filters/http/dynamo:config",
    "envoy.filters.http.squash":                                "//contrib/squash/filters/http/source:config",
    "envoy.filters.http.sxg":                                   "//contrib/sxg/filters/http/source:config",

    #
    # Network filters
    #

    "envoy.filters.network.client_ssl_auth":                    "//contrib/client_ssl_auth/filters/network/source:config",
    "envoy.filters.network.kafka_broker":                       "//contrib/kafka/filters/network/source:kafka_broker_config_lib",
    "envoy.filters.network.kafka_mesh":                         "//contrib/kafka/filters/network/source/mesh:config_lib",
    "envoy.filters.network.mysql_proxy":                        "//contrib/mysql_proxy/filters/network/source:config",
    "envoy.filters.network.postgres_proxy":                     "//contrib/postgres_proxy/filters/network/source:config",
    "envoy.filters.network.rocketmq_proxy":                     "//contrib/rocketmq_proxy/filters/network/source:config",

    #
    # Sip proxy
    #

    "envoy.filters.network.sip_proxy":                          "//contrib/sip_proxy/filters/network/source:config",
    "envoy.filters.sip.router":                                 "//contrib/sip_proxy/filters/network/source/router:config",

    #
    # Private key providers
    #

    "envoy.tls.key_providers.cryptomb":                         "//contrib/cryptomb/private_key_providers/source:config",

    #
    # Socket interface extensions
    #

    "envoy.bootstrap.vcl":                                      "//contrib/vcl/source:config",
}


ISTIO_DISABLED_EXTENSIONS = [
    # ISTIO disable tcp_stats by default because this plugin must be built and running on kernel >= 4.6
    "envoy.transport_sockets.tcp_stats",
]

ISTIO_ENABLED_CONTRIB_EXTENSIONS = [
    "envoy.filters.network.mysql_proxy",
    "envoy.filters.network.sip_proxy",
    "envoy.filters.sip.router",
    "envoy.tls.key_providers.cryptomb",
]

EXTENSIONS = dict([(k,v) for k,v in ENVOY_EXTENSIONS.items() if not k in ISTIO_DISABLED_EXTENSIONS] +
                  [(k,v) for k, v in ENVOY_CONTRIB_EXTENSIONS.items() if k in ISTIO_ENABLED_CONTRIB_EXTENSIONS])


# These can be changed to ["//visibility:public"], for  downstream builds which
# need to directly reference Envoy extensions.
EXTENSION_CONFIG_VISIBILITY = ["//visibility:public"]
EXTENSION_PACKAGE_VISIBILITY = ["//visibility:public"]
CONTRIB_EXTENSION_PACKAGE_VISIBILITY = ["//visibility:public"]
MOBILE_PACKAGE_VISIBILITY = ["//:mobile_library"]

LEGACY_ALWAYSLINK = True
