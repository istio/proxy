# python utility to seed the starlark constants in strings.bzl

print("")
print("CHAR_TO_INT = {")
for i in range(256):
    print("    \"\{o}\": {i},".format(
      o = oct(i)[2:],
      i = i,
    ))
print("}")

print("")
print("INT_TO_CHAR = [")
for i in range(256):
    print("    \"\{o}\",".format(
      o = oct(i)[2:],
    ))
print("]")

print("")
print("INT_TO_BINARY = [")
for i in range(256):
    print("    \"{b}\",".format(
      b = "{:0>8}".format(bin(i)[2:])
    ))
print("]")
