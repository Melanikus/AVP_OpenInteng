@echo off
rem release-layout.cmd <the run folder>: for acceptance-start-size-release (Frost 2026-11-18). The run's AVP_OpenInteng.ini becomes the release zip's own default (AVP_OpenInteng.ini
rem beside this file: a byte copy of dist\AVP_OpenInteng.ini, which host_editor_options_test checks) with the run's DataFolder=, Level= and IntengTools= lines after it, as a mapper adds
rem his. Only the run's own copy of the run folder is written. Gives back 0 when the file was made.
setlocal
if "%~1"=="" exit /b 2
set "RUN=%~1"
set "HERE=%~dp0"
findstr /b /i /c:"DataFolder=" /c:"Level=" /c:"IntengTools=" "%RUN%AVP_OpenInteng.ini" > "%RUN%release-layout-lines.txt"
if errorlevel 1 exit /b 1
copy /y "%HERE%AVP_OpenInteng.ini" "%RUN%AVP_OpenInteng.ini" >nul || exit /b 1
type "%RUN%release-layout-lines.txt" >> "%RUN%AVP_OpenInteng.ini" || exit /b 1
del "%RUN%release-layout-lines.txt"
exit /b 0
