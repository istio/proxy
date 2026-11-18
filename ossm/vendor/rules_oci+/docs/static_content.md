# Serving Static Content

This is useful for creating an image to serve static content, such as the output of building your
frontend javascript.

In this example we'll use the [docker nginx image](https://hub.docker.com/_/nginx), but you could
use any other static content webserver the same way.

## Example

Pull our base image.

**./WORKSPACE**

```python
load("@rules_oci//oci:pull.bzl", "oci_pull")
oci_pull(
    name = "nginx_debian_slim",
    digest = "sha256:6b06964cdbbc517102ce5e0cef95152f3c6a7ef703e4057cb574539de91f72e6",
    image = "docker.io/library/nginx",
)
```

Next lets create our static content files.

**./frontend/index.html**

```html
<!doctype html>
<html>
  <body>
    <h1>Our Homepage</h1>

    <p>Hello from index.html</p>
  </body>
</html>
```

**./frontend/textfile.txt**

```txt
This is text file.
```

And finally the build rules for our image.

**./frontend/BUILD**

```python
load("@rules_oci//oci:defs.bzl", "oci_image", "oci_load")
load("@tar.bzl", "mutate", "tar")
filegroup(
    name = "static",
    srcs = ["index.html", "textfile.txt"],
)

tar(
    name = "static_tar",
    srcs = [":static"],
    mutate = mutate(package_dir = "usr/share/nginx/html"),
)

oci_image(
    name = "frontend_image",
    base = "@nginx_debian_slim",
    tars = [
      ":static_tar",
    ],
    # Intentionally omit cmd/entrypoint to default to the base nginx container's cmd/entrypoint.
    # entrypoint = [],
    # cmd = [],
)
oci_load(
    name = "frontend_tarball",
    image = ":frontend_image",
    repo_tags = ["ourfrontend:latest"],
)


```

If you want to customize the nginx.conf you could create `./frontend/nginx.conf` and add this to
`./frontend/BUILD`.

```python

pkg_tar(
    name = "nginx_conf_tar",
    srcs = [":nginx.conf"],
    package_dir = "/etc/nginx",
)

# ...
oci_image(
  #...
  tars = [
    ":static_tar",
    ":nginx_conf_tar
  ],
  # ...
)

```

## Try running the container with docker

```bash
bazel run :frontend_tarball
docker run --rm -p 8080:80 "ourfrontend:latest"
```

Wait for nginx to start in your container, and then go to `localhost:8080` and `localhost:8080/example.txt` to see your static content.
