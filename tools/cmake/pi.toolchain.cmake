SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_VERSION 1)

# GCC 4.8.3
#SET(CMAKE_C_COMPILER $ENV{HOME}/raspberrypi/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian/bin/arm-linux-gnueabihf-gcc)
#SET(CMAKE_CXX_COMPILER $ENV{HOME}/raspberrypi/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian/bin/arm-linux-gnueabihf-g++)

SET(PI_TOOLCHAIN /opt/pi/tools/arm-bcm2708/arm-rpi-4.9.3-linux-gnueabihf)
SET(PI_ROOTFS /opt/pi/tools/rootfs)

# GCC 4.9.3
#SET(CMAKE_C_COMPILER ${PI_TOOLCHAIN}/bin/arm-linux-gnueabihf-gcc)
#SET(CMAKE_CXX_COMPILER ${PI_TOOLCHAIN}/bin/arm-linux-gnueabihf-g++)

SET(CMAKE_C_COMPILER /usr/bin/arm-linux-gnueabihf-gcc-5)
SET(CMAKE_CXX_COMPILER /usr/bin/arm-linux-gnueabihf-g++-5)

SET(CMAKE_FIND_ROOT_PATH ${PI_ROOTFS})
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
SET(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DGOOGLE_PROTOBUF_DONT_USE_UNALIGNED=1 -DHAVE_LONG_LONG=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGOOGLE_PROTOBUF_DONT_USE_UNALIGNED=1 -DHAVE_LONG_LONG=1")
