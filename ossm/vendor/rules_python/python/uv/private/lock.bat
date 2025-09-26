if defined BUILD_WORKSPACE_DIRECTORY (
    set "out=%BUILD_WORKSPACE_DIRECTORY%\{{src_out}}"
) else (
    exit /b 1
)

"{{args}}" --output-file "%out%" %*
