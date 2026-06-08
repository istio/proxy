"A trivial zipapp that prints a message"


def main():
    print("Hello from zipapp")
    try:
        import some_dep

        print(f"dep: {some_dep}")
    except ImportError:
        import sys

        print("Failed to import `some_dep`", file=sys.stderr)
        print("sys.path:", file=sys.stderr)
        for i, x in enumerate(sys.path):
            print(i, x, file=sys.stderr)
        raise


if __name__ == "__main__":
    main()
