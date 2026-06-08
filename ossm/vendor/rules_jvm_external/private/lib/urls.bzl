# Copyright 2023 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and

def split_url(url):
    protocol = url[:url.find("://")]
    url_without_protocol = url[url.find("://") + 3:]
    url_parts = url_without_protocol.split("/")
    return protocol, url_parts

_REMOTE_SCHEMES = [
    "ftp",
    "http",
    "https",
]

def scheme_and_host(url):
    if not url:
        return None

    new_url = remove_auth_from_url(url)
    (protocol, url_parts) = split_url(new_url)
    return protocol + "://" + url_parts[0]

def remove_auth_from_url(url):
    """Returns url without `user:pass@` or `user@`."""
    if "@" not in url:
        return url
    protocol, url_parts = split_url(url)
    host = url_parts[0]
    if "@" not in host:
        return url
    last_index = host.rfind("@", 0, None)
    userless_host = host[last_index + 1:]
    new_url = "{}://{}".format(protocol, "/".join([userless_host] + url_parts[1:]))
    return new_url

def extract_netrc_from_auth_url(url):
    """Return a dict showing the netrc machine, login, and password extracted from a url.

    Returns:
        A dict that is empty if there were no credentials in the url.
        A dict that has three keys -- machine, login, password -- with their respective values. These values should be
        what is needed for the netrc entry of the same name except for password whose value may be empty meaning that
        there is no password for that login.
    """
    if "@" not in url:
        return {}
    protocol, url_parts = split_url(url)
    login_password_host = url_parts[0]
    if "@" not in login_password_host:
        return {}
    login_password, host = login_password_host.rsplit("@", 1)
    login_password_split = login_password.split(":", 1)
    login = login_password_split[0]

    # If password is not provided, then this will be a 1-length split
    if len(login_password_split) < 2:
        password = None
    else:
        password = login_password_split[1]
    if not host:
        fail("Got a blank host from: {}".format(url))
    if not login:
        fail("Got a blank login from: {}".format(url))

    # Do not fail for blank password since that is sometimes a thing
    return {
        "machine": host,
        "login": login,
        "password": password,
    }

def _is_windows(repository_os):
    return repository_os.name.find("windows") != -1

def get_m2local_url(repo_os, path_func, artifact):
    if _is_windows(repo_os):
        user_home = repo_os.environ.get("USERPROFILE").replace("\\", "/")
    else:
        user_home = repo_os.environ.get("HOME")

    local_path = artifact["file"]

    if not local_path:
        # In theory, we could calculate a path, but I'm not entirely sure how we would even get here with a recent
        # version of the lock file
        return None

    if not local_path.startswith("/"):
        local_path = "%s/.m2/repository/%s" % (user_home, local_path)

    path = path_func(local_path)
    if path.exists:
        return "file://%s" % local_path
    return None
