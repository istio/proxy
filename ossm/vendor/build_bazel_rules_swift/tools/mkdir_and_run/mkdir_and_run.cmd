
@echo off

setlocal

set argc=0
FOR %%a IN (%*) DO SET /A argc+=1

IF %argc% LSS 2 (
  echo ERROR: Need at least two arguments. 1>&2
  exit /b 1
)

set location=%1
shift

set executable=%1
shift

md %location:/=\%
"%executable:/=\%" %*

endlocal
