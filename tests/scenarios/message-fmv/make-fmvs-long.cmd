@echo off
rem make-fmvs-long.cmd <data folder> <his FMVs folder>: for acceptance-strategy-message-fmv-scroll (Vasquez 2026-11-21, the owner's "We can't scroll down to see message 1001...").
rem Fills the run's own data folder's FMVs\ (run-shards' {datadir}: only that copy is written; its scenario says changes-data, so run-shards makes the shard's data afresh right after it, before a later scenario of the shard could list these files) with HIS LIST: one file for every message
rem FMV his game folder holds (53, read, never written), so the Message window's number list holds his numbers, plus message450.smk at its end (54 rows; 450 is the highest
rem number the game lets through, and the editor never lists his 1001). The editor lists a number by its file's NAME (fmv_files.cpp FmvFiles_MessageNumbers), so every file but two is a 50-byte stand-in made by fmv-hostile.js
rem beside this file (the repository holds no video; a run's folder stays small): message47.smk and message450.smk are copies of his message47.smk, a real AvP message
rem video (SMK2 128 x 96, 15 fps), so Play shows a frame of the one the test picks.
rem Gives back 0 when every file was made.
setlocal
if "%~1"=="" exit /b 2
if "%~2"=="" exit /b 2
set "DATA=%~1"
set "DATA=%DATA:/=\%"
set "HIS=%~2"
set "HERE=%~dp0"
if not exist "%DATA%\FMVs" mkdir "%DATA%\FMVs" || exit /b 1
for %%f in ("%HIS%\message*.smk") do (
	node "%HERE%fmv-hostile.js" too-short > "%DATA%\FMVs\%%~nxf" || exit /b 1
)
copy /y "%HIS%\message47.smk" "%DATA%\FMVs\message47.smk" >nul || exit /b 1
copy /y "%HIS%\message47.smk" "%DATA%\FMVs\message450.smk" >nul || exit /b 1
exit /b 0
