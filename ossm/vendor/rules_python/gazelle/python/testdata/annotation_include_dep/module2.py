# gazelle:include_dep //foo:bar

if __name__ == "__main__":
    # gazelle:include_dep //checking/py_binary/from/if:works
    print("hello")
