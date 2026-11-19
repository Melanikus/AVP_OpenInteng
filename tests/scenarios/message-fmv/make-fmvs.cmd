@echo off
rem make-fmvs.cmd <data folder> <his FMVs folder>: for acceptance-strategy-message-fmv (Dietrich 2026-11-18). Fills the run's own data folder's FMVs\ (run-shards' {datadir}:
rem only that copy is written; the next run's mirror takes it away) with three of the owner's own message FMVs, copied from his game folder (read, never written), and two
rem hostile files made by fmv-hostile.js beside this file (the repository holds no video; Bishop 11-18):
rem   message47.smk, message167.smk  his, AvP's message format (SMK2 128 x 96, 15 fps, 22050 Hz 8-bit mono)
rem   message238.smk                 his (the game's own), 11025 Hz 16-bit: not AvP's message format
rem   message165.smk                 a Bink header under a Smacker name: not a Smacker video
rem   message166.smk                 50 bytes: too short for a Smacker header
rem Gives back 0 when every file was made.
setlocal
if "%~1"=="" exit /b 2
if "%~2"=="" exit /b 2
set "DATA=%~1"
set "DATA=%DATA:/=\%"
set "HIS=%~2"
set "HERE=%~dp0"
if not exist "%DATA%\FMVs" mkdir "%DATA%\FMVs" || exit /b 1
for %%n in (message47.smk message167.smk message238.smk) do (
	copy /y "%HIS%\%%n" "%DATA%\FMVs\%%n" >nul || exit /b 1
)
node "%HERE%fmv-hostile.js" not-smacker > "%DATA%\FMVs\message165.smk" || exit /b 1
node "%HERE%fmv-hostile.js" too-short > "%DATA%\FMVs\message166.smk" || exit /b 1
exit /b 0
