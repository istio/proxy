import hashlib
import os
import shutil
import stat
import sys

BLOCK_SIZE = 256 * 1024


def create_exe_zip(preamble_path, zip_path, output_path):
    sha256_hash = hashlib.sha256()
    with open(zip_path, "rb", buffering=BLOCK_SIZE) as f:
        for byte_block in iter(lambda: f.read(BLOCK_SIZE), b""):
            sha256_hash.update(byte_block)
    zip_hash = sha256_hash.hexdigest()

    with open(preamble_path, "rb") as f:
        preamble_content = f.read()

    preamble_content = preamble_content.replace(b"%ZIP_HASH%", zip_hash.encode("utf-8"))

    with open(output_path, "wb") as out_f:
        out_f.write(preamble_content)
        with open(zip_path, "rb") as zip_f:
            shutil.copyfileobj(zip_f, out_f, length=BLOCK_SIZE)

    st = os.stat(output_path)
    os.chmod(output_path, st.st_mode | stat.S_IEXEC)


def main():
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <preamble> <zip> <output>", file=sys.stderr)
        sys.exit(1)

    create_exe_zip(sys.argv[1], sys.argv[2], sys.argv[3])


if __name__ == "__main__":
    sys.exit(main())
