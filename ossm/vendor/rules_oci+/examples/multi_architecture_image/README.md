# Multiarch OCI

## Running the example

Build and load the multiarch OCI image to Docker

```shell
bazel run //examples/multi_architecture_image:load
```

Run the container

```shell
# Use the image ID instead of the more friendly image name
docker run <IMAGE-ID>
```

> [!TIP]
> Find the image ID with the following command:
> ```shell
> # Display the information about the image you just built
> docker image ls | grep my-repository:latest
> ```

## Troubleshotting

> [!WARNING]
> By default, Docker does not support loading oci tarballs into the deamon. So,
> running this example could result in something like:
>
> ```shell
> bazel run src/app:tarball_multiarch
> open /var/lib/docker/tmp/docker-import-1480447137/blobs/json: no such file or directory
> ```

From the [documentation](https://docs.docker.com/engine/storage/containerd/#:~:text=Enable%20containerd%20image%20store%20on%20Docker%20Engine,take%20effect.%20$%20sudo%20systemctl%20restart%20docker.) we can see that there's a way to change the Docker Engine drive to be `containerd`, which will solve this issue.

> While the `overlay2` driver still remains the default driver for Docker Engine, you can opt in to using containerd snapshotters as an experimental feature.

### Fix for most linux distros

> [!NOTE]
> The following is just a copy-paste of the docker documentation. Please refer to it if in doubt.

The following steps explain how to enable the containerd snapshotters feature.

1. Add the following configuration to your `/etc/docker/daemon.json` configuration file:

```shell
{
  "features": {
    "containerd-snapshotter": true
  }
}
```

2. Save the file.

3. Restart the daemon for the changes to take effect.

```shell
sudo systemctl restart docker
```

After restarting the daemon, running `docker info` shows that you're using containerd snapshotter storage drivers.

```shell
$ docker info -f '{{ .DriverStatus }}'
[[driver-type io.containerd.snapshotter.v1]]
```

Docker Engine uses `overlayfs` containerd snapshotter by default.

### Fix for NixOS

In NixOS, from some exploration one arrives to the NixOS Packages Options documetation where the [virtualisation.docker.daemon.settings](https://search.nixos.org/options?channel=24.11&show=virtualisation.docker.daemon.settings&from=0&size=50&sort=relevance&type=packages&query=virtualisation.docker) allows us to write the `daemon.json` file.

In conclusion, you need to add the following to your `configuration.nix` file:

```
  virtualisation = {
    containerd.enable = true;
    docker = {
      enable = true;
      daemon.settings = {
        features = {
          containerd-snapshotter = true;
        };
      };
    };
  };

  users.users.your-username.extraGroups = ["docker"];
```
