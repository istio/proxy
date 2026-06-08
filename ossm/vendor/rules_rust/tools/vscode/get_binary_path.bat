@echo off
setlocal enabledelayedexpansion

REM Check if an argument was provided
if "%~1"=="" (
    echo Usage: %~nx0 ^<path^> >&2
    exit /b 1
)

REM Get the input path and convert to absolute path
set "input_path=%~f1"

REM Check if the path contains "bazel-out"
echo !input_path! | findstr /C:"bazel-out" >nul
if errorlevel 1 (
    echo Error: Path does not contain 'bazel-out' >&2
    exit /b 1
)

REM Extract everything from "bazel-out" onwards
for /f "tokens=1,* delims==" %%a in ("!input_path:bazel-out=^=bazel-out!") do (
    set "result=%%b"
)

REM Strip .runfiles wrapper path if present to get actual binary
REM bazel-out\...\bin\foo\bar.runfiles\_main\foo\bar -> bazel-out\...\bin\foo\bar
echo !result! | findstr /C:".runfiles\" >nul
if not errorlevel 1 (
    for /f "tokens=1 delims=." %%a in ("!result:.runfiles=^.runfiles!") do (
        set "result=%%a"
    )
)

echo !result! 1>&2
endlocal
