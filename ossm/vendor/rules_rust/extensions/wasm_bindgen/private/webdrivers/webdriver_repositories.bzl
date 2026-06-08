"""Depednencies for `wasm_bindgen_test` rules"""

load("//private/webdrivers/chrome:webdriver_chrome.bzl", "chrome_deps")
load("//private/webdrivers/firefox:webdriver_firefox.bzl", "firefox_deps")
load("//private/webdrivers/safari:webdriver_safari.bzl", "safari_deps")

def webdriver_repositories():
    """Define webdriver repositories for bzlmod

    Returns:
        list: A list of repository structs.
    """
    direct_deps = []
    direct_deps.extend(chrome_deps())
    direct_deps.extend(firefox_deps())
    direct_deps.extend(safari_deps())

    return direct_deps
