#include "platform_util.h"

#include <cassert>
#include <cstdint>
#include <fstream>
#include <regex>

// clang-format off
#if defined(__x86_64__) || defined(_M_X64)
#  define DD_SDK_CPU_ARCH "x86_64"
#elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
#  define DD_SDK_CPU_ARCH "x86"
#elif defined(__aarch64__) || defined(_M_ARM64)
#  define DD_SDK_CPU_ARCH "arm64"
#else
#  define DD_SDK_CPU_ARCH "unknown"
#endif

#if defined(__APPLE__) || defined(__linux__) || defined(__unix__)
#  include <pthread.h>
#  include <sys/types.h>
#  include <sys/utsname.h>
#  include <unistd.h>
#  if defined(__APPLE__)
#    include <sys/sysctl.h>
#    define DD_SDK_OS "Darwin"
#    define DD_SDK_KERNEL "Darwin"
#  elif defined(__linux__) || defined(__unix__)
#    define DD_SDK_OS "GNU/Linux"
#    define DD_SDK_KERNEL "Linux"
#    include "string_util.h"
#    include <errno.h>
#    include <fstream>
#    include <fcntl.h>
#    include <sys/types.h>
#    include <sys/mman.h>
#    include <sys/stat.h>
#    include <sys/statfs.h>
#  endif
#elif defined(_MSC_VER)
#  include <windows.h>
#  include <processthreadsapi.h>
#  include <winsock.h>
#endif
// clang-format on

namespace datadog {
namespace tracing {
namespace {

#if defined(__APPLE__)
std::string get_os_version() {
  char os_version[20] = "";
  size_t len = sizeof(os_version);

  sysctlbyname("kern.osproductversion", os_version, &len, NULL, 0);
  return os_version;
}
#elif defined(__linux__)
std::string get_os_version() {
  std::ifstream os_release_file("/etc/os-release");
  if (!os_release_file.is_open()) {
    return "";
  }

  std::string line;

  while (std::getline(os_release_file, line)) {
    size_t pos = line.find('=');
    if (pos == std::string::npos) {
      continue;
    }

    std::string key = line.substr(0, pos);
    to_lower(key);
    if (key == "version") {
      std::string value = line.substr(pos + 1);
      return value;
    }
  }

  return "";
}
#endif

#if defined(__APPLE__) || defined(__linux__) || defined(__unix__)
HostInfo _get_host_info() {
  HostInfo res;

  struct utsname buffer;
  if (uname(&buffer) != 0) {
    return res;
  }

  res.os = DD_SDK_OS;
  res.os_version = get_os_version();
  res.hostname = buffer.nodename;
  res.cpu_architecture = DD_SDK_CPU_ARCH;
  res.kernel_name = DD_SDK_KERNEL;
  res.kernel_version = buffer.version;
  res.kernel_release = buffer.release;

  return res;
}
#elif defined(_MSC_VER)
std::tuple<std::string, std::string> get_windows_info() {
  // NOTE(@dmehala): Retrieving the Windows version has been complicated since
  // Windows 8.1. The `GetVersion` function and its variants depend on the
  // application manifest, which is the lowest version supported by the
  // application. Use `RtlGetVersion` to obtain the accurate OS version
  // regardless of the manifest.
  using RtlGetVersion = auto(*)(LPOSVERSIONINFOEXW)->NTSTATUS;

  RtlGetVersion func =
      (RtlGetVersion)GetProcAddress(GetModuleHandleA("ntdll"), "RtlGetVersion");

  if (func) {
    OSVERSIONINFOEXW os_info;
    ZeroMemory(&os_info, sizeof(OSVERSIONINFO));
    os_info.dwOSVersionInfoSize = sizeof(os_info);

    if (func(&os_info) == 0) {
      switch (os_info.dwMajorVersion) {
        case 5: {
          switch (os_info.dwMinorVersion) {
            case 0:
              return {"Windows 2000", "NT 5.0"};
            case 1:
              return {"Windows XP", "NT 5.1"};
            case 2:
              return {"Windows XP", "NT 5.2"};
            default:
              return {"Windows XP", "NT 5.x"};
          }
        }; break;
        case 6: {
          switch (os_info.dwMinorVersion) {
            case 0:
              return {"Windows Vista", "NT 6.0"};
            case 1:
              return {"Windows 7", "NT 6.1"};
            case 2:
              return {"Windows 8", "NT 6.2"};
            case 3:
              return {"Windows 8.1", "NT 6.3"};
            default:
              return {"Windows 8.1", "NT 6.x"};
          }
        }; break;
        case 10: {
          if (os_info.dwBuildNumber >= 10240 && os_info.dwBuildNumber < 22000) {
            return {"Windows 10", "NT 10.0"};
          } else if (os_info.dwBuildNumber >= 22000) {
            return {"Windows 11", "21H2"};
          }
        }; break;
      }
    }
  }

  return {"", ""};
}

HostInfo _get_host_info() {
  HostInfo host;
  host.cpu_architecture = DD_SDK_CPU_ARCH;

  auto [os, os_version] = get_windows_info();
  host.os = std::move(os);
  host.os_version = std::move(os_version);

  char buffer[256];
  if (0 == gethostname(buffer, sizeof(buffer))) {
    host.hostname = buffer;
  }

  return host;
}
#else
HostInfo _get_host_info() {
  HostInfo res;
  return res;
}
#endif

}  // namespace

HostInfo get_host_info() {
  static const HostInfo host_info = _get_host_info();
  return host_info;
}

std::string get_hostname() { return get_host_info().hostname; }

int get_process_id() {
#if defined(_MSC_VER)
  return GetCurrentProcessId();
#else
  return ::getpid();
#endif
}

std::string get_process_name() {
#if defined(__APPLE__) || defined(__FreeBSD__)
  const char* process_name = getprogname();
  return (process_name != nullptr) ? process_name : "unknown-service";
#elif defined(__linux__) || defined(__unix__)
  return program_invocation_short_name;
#elif defined(_MSC_VER)
  TCHAR exe_name[MAX_PATH];
  if (GetModuleFileName(NULL, exe_name, MAX_PATH) <= 0) {
    return "unknown-service";
  }
#ifdef UNICODE
  std::wstring wStr(exe_name);
  std::string path = std::string(wStr.begin(), wStr.end());
#else
  std::string path = std::string(exe_name);
#endif
  return path.substr(path.find_last_of("/\\") + 1);
#else
  return "unknown-service";
#endif
}

int at_fork_in_child(void (*on_fork)()) {
#if defined(_MSC_VER)
  // Windows does not have `fork`, and so this is not relevant there.
  (void)on_fork;
  return 0;
#else
  // https://pubs.opengroup.org/onlinepubs/9699919799/functions/pthread_atfork.html
  return pthread_atfork(/*before fork*/ nullptr, /*in parent*/ nullptr,
                        /*in child*/ on_fork);
#endif
}

InMemoryFile::InMemoryFile(void* handle) : handle_(handle) {}

InMemoryFile::InMemoryFile(InMemoryFile&& rhs) {
  std::swap(rhs.handle_, handle_);
}

InMemoryFile& InMemoryFile::operator=(InMemoryFile&& rhs) {
  std::swap(handle_, rhs.handle_);
  return *this;
}

#if defined(__linux__) || defined(__unix__)

InMemoryFile::~InMemoryFile() {
  /// NOTE(@dmehala): No need to close the fd since it is automatically handled
  /// by `MFD_CLOEXEC`.
  if (handle_ == nullptr) return;
  int* data = static_cast<int*>(handle_);
  close(*data);
  delete (data);
}

bool InMemoryFile::write_then_seal(const std::string& data) {
  int fd = *static_cast<int*>(handle_);

  size_t written = write(fd, data.data(), data.size());
  if (written != data.size()) return false;

  return fcntl(fd, F_ADD_SEALS,
               F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE | F_SEAL_SEAL) == 0;
}

Expected<InMemoryFile> InMemoryFile::make(StringView name) {
  int fd = memfd_create(name.data(), MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (fd == -1) {
    std::string err_msg = "failed to create an anonymous file. errno = ";
    err_msg += std::to_string(errno);
    return Error{Error::Code::OTHER, std::move(err_msg)};
  }

  int* handle = new int;
  *handle = fd;
  return InMemoryFile(handle);
}

#else
InMemoryFile::~InMemoryFile() {}
bool InMemoryFile::write_then_seal(const std::string&) { return false; }
Expected<InMemoryFile> InMemoryFile::make(StringView) {
  return Error{Error::Code::NOT_IMPLEMENTED, "In-memory file not implemented"};
}
#endif

namespace container {
namespace {
#if defined(__linux__) || defined(__unix__)
/// Magic numbers from linux/magic.h:
/// <https://github.com/torvalds/linux/blob/ca91b9500108d4cf083a635c2e11c884d5dd20ea/include/uapi/linux/magic.h#L71>
constexpr uint64_t TMPFS_MAGIC = 0x01021994;
constexpr uint64_t CGROUP_SUPER_MAGIC = 0x27e0eb;
constexpr uint64_t CGROUP2_SUPER_MAGIC = 0x63677270;

/// Magic number from linux/proc_ns.h:
/// <https://github.com/torvalds/linux/blob/5859a2b1991101d6b978f3feb5325dad39421f29/include/linux/proc_ns.h#L41-L49>
constexpr ino_t HOST_CGROUP_NAMESPACE_INODE = 0xeffffffb;

/// Represents the cgroup version of the current process.
enum class Cgroup : char { v1, v2 };

Optional<ino_t> get_inode(std::string_view path) {
  struct stat buf;
  if (stat(path.data(), &buf) != 0) {
    return nullopt;
  }

  return buf.st_ino;
}

// Host namespace inode number are hardcoded, which allows for dectection of
// whether the binary is running in host or not. However, it does not work when
// running in a Docker in Docker environment.
bool is_running_in_host_namespace() {
  // linux procfs file that represents the cgroup namespace of the current
  // process.
  if (auto inode = get_inode("/proc/self/ns/cgroup")) {
    return *inode == HOST_CGROUP_NAMESPACE_INODE;
  }

  return false;
}

Optional<Cgroup> get_cgroup_version() {
  struct statfs buf;

  if (statfs("/sys/fs/cgroup", &buf) != 0) {
    return nullopt;
  }

  if (buf.f_type == CGROUP_SUPER_MAGIC || buf.f_type == TMPFS_MAGIC)
    return Cgroup::v1;
  else if (buf.f_type == CGROUP2_SUPER_MAGIC)
    return Cgroup::v2;

  return nullopt;
}

Optional<std::string> find_container_id_from_cgroup() {
  auto cgroup_fd = std::ifstream("/proc/self/cgroup", std::ios::in);
  if (!cgroup_fd.is_open()) return nullopt;

  return find_container_id(cgroup_fd);
}
#endif
}  // namespace

Optional<std::string> find_container_id(std::istream& source) {
  std::string line;

  // Look for Docker container IDs in the basic format: `docker-<uuid>.scope`.
  constexpr std::string_view docker_str = "docker-";

  while (std::getline(source, line)) {
    // Example:
    // `0::/system.slice/docker-abcdef0123456789abcdef0123456789.scope`
    if (auto beg = line.find(docker_str); beg != std::string::npos) {
      beg += docker_str.size();
      auto end = line.find(".scope", beg);
      if (end == std::string::npos || end - beg <= 0) {
        continue;
      }

      auto container_id = line.substr(beg, end - beg);
      return container_id;
    }
  }

  // Reset the stream to the beginning.
  source.clear();
  source.seekg(0);

  // Perform a second pass using a regular expression for matching container IDs
  // in a Fargate environment. This two-step approach is used because STL
  // `regex` is relatively slow, so we avoid using it unless necessary.
  static const std::string uuid_regex_str =
      "[0-9a-f]{8}[-_][0-9a-f]{4}[-_][0-9a-f]{4}[-_][0-9a-f]{4}[-_][0-9a-f]{12}"
      "|(?:[0-9a-f]{8}(?:-[0-9a-f]{4}){4}$)";
  static const std::string container_regex_str = "[0-9a-f]{64}";
  static const std::string task_regex_str = "[0-9a-f]{32}-\\d+";
  static const std::regex path_reg("(?:.+)?(" + uuid_regex_str + "|" +
                                   container_regex_str + "|" + task_regex_str +
                                   ")(?:\\.scope)?$");

  while (std::getline(source, line)) {
    // Example:
    // `0::/system.slice/docker-abcdef0123456789abcdef0123456789.scope`
    std::smatch match;
    if (std::regex_match(line, match, path_reg) && match.size() == 2) {
      assert(match.ready());
      assert(match.size() == 2);

      return match.str(1);
    }
  }

  return nullopt;
}

Optional<ContainerID> get_id() {
#if defined(__linux__) || defined(__unix__)
  auto maybe_cgroup = get_cgroup_version();
  if (!maybe_cgroup) return nullopt;

  ContainerID id;
  switch (*maybe_cgroup) {
    case Cgroup::v1: {
      if (auto maybe_id = find_container_id_from_cgroup()) {
        id.value = *maybe_id;
        id.type = ContainerID::Type::container_id;
        break;
      }
    }
      // NOTE(@dmehala): failed to find the container ID, try getting the cgroup
      // inode.
      [[fallthrough]];
    case Cgroup::v2: {
      if (!is_running_in_host_namespace()) {
        auto maybe_inode = get_inode("/sys/fs/cgroup");
        if (maybe_inode) {
          id.type = ContainerID::Type::cgroup_inode;
          id.value = std::to_string(*maybe_inode);
        }
      }
    }; break;
  }

  return id;
#else
  return nullopt;
#endif
}

}  // namespace container

}  // namespace tracing
}  // namespace datadog
