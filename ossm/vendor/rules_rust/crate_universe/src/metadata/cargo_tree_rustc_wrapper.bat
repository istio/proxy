@ECHO OFF
@REM
@REM For details, see:
@REM  `@rules_rust//crate_universe/src/metadata/cargo_tree_resolver.rs - TreeResolver::create_rustc_wrapper`

SETLOCAL ENABLEDELAYEDEXPANSION

@REM When cargo is detecting the host configuration, the host target needs to be
@REM injected into the command.
echo %*| FINDSTR /R /C:".*rustc[\.exe\"\"]* - --crate-name ___ " | FINDSTR /V /C:"--target" >NUL
if %errorlevel%==0 (
    %* --target %HOST_TRIPLE%
    exit /b
)

@REM When querying info about the compiler, ensure the triple is mocked out to be
@REM the desired target triple for the host.
echo %*| FINDSTR /R /C:".*rustc[\.exe\"\"]* -[vV][vV]$" >NUL
if %errorlevel%==0 (

    @REM TODO: The exit code is lost here. It should be captured and explicitly
    @REM returned.
    for /F "delims=" %%i in ('%*') do (
        echo %%i| FINDSTR /R /C:"^host:" >NUL
        if errorlevel 1 (
            echo %%i
        ) else (
            echo host: %HOST_TRIPLE%
        )
    )

    exit /b
)

@REM No unique calls intercepted. Simply call rustc.exe as normal.
%*
exit /b
