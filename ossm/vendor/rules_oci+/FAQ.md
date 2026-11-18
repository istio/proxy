### My image results in a different `sha256` digest but I have not changed anything

This usually happens due to non-deterministic inputs in your image build. Common causes include:

- **Unstable tar target**: If your image includes a `.tar` file, it may embed unstable metadata such as `mtime`, `ctime`, or file ordering. Ensure tar files are created deterministically.
- **Stamped targets in metadata**: If you use a Bazel target with `stamp = 1` as input to `labels` or `annotations` in `oci_image`, it will inject non-reproducible data (e.g. build timestamp). To avoid this:
  - Set `stamp = -1` on the target (only stamped when `--stamp` is passed).
  - Do not use always-stamped targets unless intentionally including build metadata.

To compare two image outputs and understand what changed, use [`diffoci`](https://github.com/reproducible-containers/diffoci).
