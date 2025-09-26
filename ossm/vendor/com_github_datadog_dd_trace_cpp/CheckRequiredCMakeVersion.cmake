# "bin/install-cmake" loads this file with "cmake -P [...]" to check whether the
# installed version of cmake is recent enough.
#
# If "cmake -P [...]" exits with status code zero, then the versions are
# compatible.
#
# If it exits with another status code, then either the versions are
# incompatible or something else went wrong.
#
# Note: Make sure that this version is the same as that in "./CMakeLists.txt".
cmake_minimum_required(VERSION 3.24)
