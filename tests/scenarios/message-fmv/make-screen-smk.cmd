@echo off
rem make-screen-smk.cmd <data folder>: for acceptance-fmv-screen-still (Dietrich 2026-11-18). Puts a file named FMVs\triggered_plot.smk in the run's own data folder (run-shards'
rem {datadir}: only that copy is written), the name the game makes from the video screen picture FMVs\triggered_plot.RIM; with it there, ScanImagesForFMVs skips the picture and the
rem screen stays still. Its bytes don't matter (the game only opens it): fmv-hostile.js beside this file makes them. Gives back 0 when made.
setlocal
if "%~1"=="" exit /b 2
set "DATA=%~1"
set "DATA=%DATA:/=\%"
set "HERE=%~dp0"
if not exist "%DATA%\FMVs" mkdir "%DATA%\FMVs" || exit /b 1
node "%HERE%fmv-hostile.js" too-short > "%DATA%\FMVs\triggered_plot.smk" || exit /b 1
exit /b 0
