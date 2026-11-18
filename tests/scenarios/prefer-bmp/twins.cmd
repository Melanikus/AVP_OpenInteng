@echo off
rem twins.cmd <data folder>: for acceptance-hub-prefer-bmp (Frost 2026-11-18). Puts a magenta picture, made by magenta.js beside this file (the repository
rem holds no picture; Bishop 11-18), in the data folder's graphics\Envrnmts\Invasion as
rem "<picture>.bmp" for each of Invasion's own pictures named in invasion-pictures.txt (read from its BMPNAMES), so with Prefer .BMP over .RIM on every one of them is
rem read from the .bmp. Only the run's own copy of the data folder is written (run-shards' {datadir}). Gives back 0 when every copy was made.
setlocal
if "%~1"=="" exit /b 2
set "DATA=%~1"
set "DATA=%DATA:/=\%"
set "HERE=%~dp0"
if not exist "%DATA%\graphics\Envrnmts\Invasion" mkdir "%DATA%\graphics\Envrnmts\Invasion" || exit /b 1
set "PIC=%TEMP%\prefer-bmp-magenta-%RANDOM%.bmp"
node "%HERE%magenta.js" > "%PIC%" || exit /b 1
for /f "usebackq delims=" %%n in ("%HERE%invasion-pictures.txt") do (
	copy /y "%PIC%" "%DATA%\graphics\Envrnmts\Invasion\%%n.bmp" >nul || (del "%PIC%" & exit /b 1)
)
del "%PIC%"
exit /b 0
