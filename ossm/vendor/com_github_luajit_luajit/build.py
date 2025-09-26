#!/usr/bin/env python3

import argparse
import os
import shutil

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--prefix")
    args = parser.parse_args()
    src_dir = os.path.dirname(os.path.realpath(__file__))
    shutil.copytree(src_dir, os.path.basename(src_dir))
    os.chdir(os.path.basename(src_dir))

    os.environ["MACOSX_DEPLOYMENT_TARGET"] = "10.8"
    os.environ["DEFAULT_CC"] = os.environ.get("CC", "")
    os.environ["TARGET_CFLAGS"] = os.environ.get("CFLAGS", "") + " -fno-function-sections -fno-data-sections"
    os.environ["TARGET_LDFLAGS"] = os.environ.get("CFLAGS", "") + " -fno-function-sections -fno-data-sections"
    os.environ["CFLAGS"] = ""
    os.environ["LDFLAGS"] = ""

    # Don't strip the binary - it doesn't work when cross-compiling, and we don't use it anyway.
    os.environ["TARGET_STRIP"] = "@echo"

    # Remove LuaJIT from ASAN for now.
    # TODO(htuch): Remove this when https://github.com/envoyproxy/envoy/issues/6084 is resolved.
    if "ENVOY_CONFIG_ASAN" in os.environ or "ENVOY_CONFIG_MSAN" in os.environ:
      os.environ["TARGET_CFLAGS"] += " -fsanitize-blacklist=%s/com_github_luajit_luajit/clang-asan-blocklist.txt" % os.environ["PWD"]
      with open("clang-asan-blocklist.txt", "w") as f:
        f.write("fun:*\n")

    os.system('"{}" -j{} V=1 PREFIX="{}" install'.format(os.environ["MAKE"], os.cpu_count(), args.prefix))

def win_main():
    src_dir = os.path.dirname(os.path.realpath(__file__))
    dst_dir = os.getcwd() + "/luajit"
    shutil.copytree(src_dir, os.path.basename(src_dir))
    os.chdir(os.path.basename(src_dir) + "/src")
    os.system('msvcbuild.bat ' + os.getenv('WINDOWS_DBG_BUILD', '') + ' static')
    os.makedirs(dst_dir + "/lib", exist_ok=True)
    shutil.copy("lua51.lib", dst_dir + "/lib")
    os.makedirs(dst_dir + "/include/luajit-2.1", exist_ok=True)
    for header in ["lauxlib.h", "luaconf.h", "lua.h", "lua.hpp", "luajit.h", "lualib.h"]:
      shutil.copy(header, dst_dir + "/include/luajit-2.1")
    os.makedirs(dst_dir + "/bin", exist_ok=True)
    shutil.copy("luajit.exe", dst_dir + "/bin")

if os.name == 'nt':
  win_main()
else:
  main()

