### LLVM Troubleshooting

On older linux distributions (Ubuntu 16.04) you may encounter an error that C++ versions before C++ 14 are no longer
supported. In this case, just install gcc version 7 or newer. This is rare corner case, but there are gcc backports for
older distributions, so please upgrade your compiler if you ever see this error.

On Ubuntu 20.04 you may see an error that a shared library called libtinfo.so.5 is missing. In that case, just install
libtinfo via apt-get since its in the official 20.04 repo. To so, open a terminal and type:

`
apt update && apt install -y libtinfo5
`

The libtinfo5 library may have different package names on other distributions, but it is a well known
issue. [See this SO discussion](https://stackoverflow.com/questions/48674104/clang-error-while-loading-shared-libraries-libtinfo-so-5-cannot-open-shared-o)
for various solutions.

On MacOX, it is sufficient to have the Apple Clang compiler installed.
I don't recommend installing the full Xcode package unless you're developing software for an Apple device. Instead, the
Xcode Command Line Tools provide everything you need at a much smaller download size. In most cases, a simple:

`xcode-select --install`

From a terminal triggers the installation process. For details and alternative
options, [read this article on freebootcamp.](https://www.freecodecamp.org/news/install-xcode-command-line-tools/)

Windows is not directly supported, but you can use Linux on Windows with WSL to setup an Ubuntu environment within
Windows. Please refer to
the [official WSL documentation for details.](https://learn.microsoft.com/en-us/windows/wsl/install)