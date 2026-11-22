@echo off
rem make-fmv-his-film.cmd <data folder> <his Steam FMVs folder>: for acceptance-strategy-message-fmv-long (Vasquez 2026-11-21, the owner's "her video is long so it got stuck for a
rem few seconds, we need a loading video %"). Copies HIS film, message1.smk from his Steam game's FMVs (read, never written: 164.7 s, 2,471 frames of SMK2 128 x 96 at 15 a second,
rem 22050 Hz 8-bit mono, 21,844,593 bytes, SHA-256 2a2985c8...), into the run's own data folder's FMVs\ (run-shards' {datadir}: only that copy is written; the next run's mirror
rem takes it away). The repository holds no video.
rem Gives back 0 when the file was copied.
setlocal
if "%~1"=="" exit /b 2
if "%~2"=="" exit /b 2
set "DATA=%~1"
set "DATA=%DATA:/=\%"
set "HIS=%~2"
if not exist "%DATA%\FMVs" mkdir "%DATA%\FMVs" || exit /b 1
copy /y "%HIS%\message1.smk" "%DATA%\FMVs\message1.smk" >nul || exit /b 1
exit /b 0
