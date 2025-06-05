<!-- Generated with Stardoc: http://skydoc.bazel.build -->

Public API

<a id="patch"></a>

## patch

<pre>
patch(<a href="#patch-ctx">ctx</a>, <a href="#patch-patches">patches</a>, <a href="#patch-patch_cmds">patch_cmds</a>, <a href="#patch-patch_cmds_win">patch_cmds_win</a>, <a href="#patch-patch_tool">patch_tool</a>, <a href="#patch-patch_args">patch_args</a>, <a href="#patch-auth">auth</a>, <a href="#patch-patch_directory">patch_directory</a>)
</pre>

Implementation of patching an already extracted repository.

This rule is intended to be used in the implementation function of
a repository rule. If the parameters `patches`, `patch_tool`,
`patch_args`, `patch_cmds` and `patch_cmds_win` are not specified
then they are taken from `ctx.attr`.


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="patch-ctx"></a>ctx |  The repository context of the repository rule calling this utility function.   |  none |
| <a id="patch-patches"></a>patches |  The patch files to apply. List of strings, Labels, or paths.   |  `None` |
| <a id="patch-patch_cmds"></a>patch_cmds |  Bash commands to run for patching, passed one at a time to bash -c. List of strings   |  `None` |
| <a id="patch-patch_cmds_win"></a>patch_cmds_win |  Powershell commands to run for patching, passed one at a time to powershell /c. List of strings. If the boolean value of this parameter is false, patch_cmds will be used and this parameter will be ignored.   |  `None` |
| <a id="patch-patch_tool"></a>patch_tool |  Path of the patch tool to execute for applying patches. String.   |  `None` |
| <a id="patch-patch_args"></a>patch_args |  Arguments to pass to the patch tool. List of strings.   |  `None` |
| <a id="patch-auth"></a>auth |  An optional dict specifying authentication information for some of the URLs.   |  `None` |
| <a id="patch-patch_directory"></a>patch_directory |  Directory to apply the patches in   |  `None` |


<a id="repo_utils.get_env_var"></a>

## repo_utils.get_env_var

<pre>
repo_utils.get_env_var(<a href="#repo_utils.get_env_var-rctx">rctx</a>, <a href="#repo_utils.get_env_var-name">name</a>, <a href="#repo_utils.get_env_var-default">default</a>)
</pre>

Find an environment variable in system. Doesn't %-escape the value!

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="repo_utils.get_env_var-rctx"></a>rctx |  rctx   |  none |
| <a id="repo_utils.get_env_var-name"></a>name |  environment variable name   |  none |
| <a id="repo_utils.get_env_var-default"></a>default |  default value to return if env var is not set in system   |  none |

**RETURNS**

The environment variable value or the default if it is not set


<a id="repo_utils.get_home_directory"></a>

## repo_utils.get_home_directory

<pre>
repo_utils.get_home_directory(<a href="#repo_utils.get_home_directory-rctx">rctx</a>)
</pre>



**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="repo_utils.get_home_directory-rctx"></a>rctx |  <p align="center"> - </p>   |  none |


<a id="repo_utils.is_darwin"></a>

## repo_utils.is_darwin

<pre>
repo_utils.is_darwin(<a href="#repo_utils.is_darwin-rctx">rctx</a>)
</pre>

Returns true if the host operating system is Darwin

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="repo_utils.is_darwin-rctx"></a>rctx |  <p align="center"> - </p>   |  none |


<a id="repo_utils.is_linux"></a>

## repo_utils.is_linux

<pre>
repo_utils.is_linux(<a href="#repo_utils.is_linux-rctx">rctx</a>)
</pre>

Returns true if the host operating system is Linux

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="repo_utils.is_linux-rctx"></a>rctx |  <p align="center"> - </p>   |  none |


<a id="repo_utils.is_windows"></a>

## repo_utils.is_windows

<pre>
repo_utils.is_windows(<a href="#repo_utils.is_windows-rctx">rctx</a>)
</pre>

Returns true if the host operating system is Windows

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="repo_utils.is_windows-rctx"></a>rctx |  <p align="center"> - </p>   |  none |


<a id="repo_utils.os"></a>

## repo_utils.os

<pre>
repo_utils.os(<a href="#repo_utils.os-rctx">rctx</a>)
</pre>

Returns the name of the host operating system

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="repo_utils.os-rctx"></a>rctx |  rctx   |  none |

**RETURNS**

The string "windows", "linux", "freebsd" or "darwin" that describes the host os


<a id="repo_utils.platform"></a>

## repo_utils.platform

<pre>
repo_utils.platform(<a href="#repo_utils.platform-rctx">rctx</a>)
</pre>

Returns a normalized name of the host os and CPU architecture.

Alias archictures names are normalized:

x86_64 => amd64
aarch64 => arm64

The result can be used to generate repository names for host toolchain
repositories for toolchains that use these normalized names.

Common os & architecture pairs that are returned are,

- darwin_amd64
- darwin_arm64
- linux_amd64
- linux_arm64
- linux_s390x
- linux_ppc64le
- windows_amd64


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="repo_utils.platform-rctx"></a>rctx |  rctx   |  none |

**RETURNS**

The normalized "<os>_<arch>" string of the host os and CPU architecture.


