import pathlib
import sys
import zlib


def main(args):
    in_path = pathlib.Path(args.pop(0))
    out_path = pathlib.Path(args.pop(0))

    data = in_path.read_bytes()
    offset = 0
    for _ in range(4):
        offset = data.index(b"\n", offset) + 1

    compressed_bytes = zlib.compress(data[offset:])
    with out_path.open(mode="bw") as fp:
        fp.write(data[:offset])
        fp.write(compressed_bytes)

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
