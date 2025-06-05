@ECHO OFF
SETLOCAL enabledelayedexpansion

SET command=%*

:: Resolve the `${pwd}` placeholders
SET command=!command:${pwd}=%CD%!

:: Strip out the leading `--` argument.
SET command=!command:~3!

:: Find the rustc.exe argument and sanitize it's path
for %%A in (%*) do (
    SET arg=%%~A
    if "!arg:~-9!"=="rustc.exe" (
        SET sanitized=!arg:/=\!

        SET command=!sanitized! !command:%%~A=!
        goto :break
    )
)

:break

%command%

:: Capture the exit code of rustc.exe
SET exit_code=!errorlevel!

:: Exit with the same exit code
EXIT /b %exit_code%
