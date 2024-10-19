@ECHO OFF
SETLOCAL

for /f "delims=" %%a in ('WHERE 7z 2^>nul') do set "zip=%%a"

if "%zip%" == "" (
	if exist "C:\Program Files\7-Zip\7z.exe" (
		set "zip=C:\Program Files\7-Zip\7z.exe"
	) else (
		echo can not find 7z.exe
		goto :EndErr
	)
)

set ZipFileName=project64
set VSPlatform=win32
if not "%1" == "" set ZipFileName=%1
if not "%2" == "" set VSPlatform=%2

SET current_dir=%cd%
cd /d %~dp0..\..\
SET base_dir=%cd%
cd /d %current_dir%

IF EXIST "%base_dir%\Package" rmdir /S /Q "%base_dir%\Package"
IF %ERRORLEVEL% NEQ 0 GOTO EndErr
IF NOT EXIST "%base_dir%\Package" mkdir "%base_dir%\Package"
IF %ERRORLEVEL% NEQ 0 GOTO EndErr

rd "%base_dir%\Bin\Package" /Q /S > NUL 2>&1
md "%base_dir%\Bin\Package\"
md "%base_dir%\Bin\Package\Config\"
md "%base_dir%\Bin\Package\Config\Cheats\"
md "%base_dir%\Bin\Package\Config\Enhancements\"
md "%base_dir%\Bin\Package\Lang\"
md "%base_dir%\Bin\Package\Plugin\"
md "%base_dir%\Bin\Package\Plugin\Audio\"
md "%base_dir%\Bin\Package\Plugin\GFX\"
md "%base_dir%\Bin\Package\Plugin\Input\"
md "%base_dir%\Bin\Package\Plugin\RSP\"

IF EXIST "%base_dir%\Plugin\%VSPlatform%\GFX\GLideN64" (
md "%base_dir%\Bin\Package\Plugin\GFX\GLideN64"
copy "%base_dir%\Plugin\%VSPlatform%\GFX\GLideN64\GLideN64.dll" "%base_dir%\Bin\Package\Plugin\GFX\GLideN64\GLideN64.dll"
copy "%base_dir%\Plugin\%VSPlatform%\GFX\GLideN64\*.ini" "%base_dir%\Bin\Package\Plugin\GFX\GLideN64"
md "%base_dir%\Bin\Package\Plugin\GFX\GLideN64\translations"
copy "%base_dir%\Plugin\%VSPlatform%\GFX\GLideN64\translations\*.Lang" "%base_dir%\Bin\Package\Plugin\GFX\GLideN64\translations"
)

copy "%base_dir%\Bin\%VSPlatform%\Release\Project64.exe" "%base_dir%\Bin\Package"
copy "%base_dir%\Config\Video.rdb" "%base_dir%\Bin\Package\Config"
copy "%base_dir%\Config\Audio.rdb" "%base_dir%\Bin\Package\Config"
copy "%base_dir%\Config\Cheats\*.cht" "%base_dir%\Bin\Package\Config\Cheats"
copy "%base_dir%\Config\Enhancements\*.enh" "%base_dir%\Bin\Package\Config\Enhancements"
copy "%base_dir%\Config\Project64.rdb" "%base_dir%\Bin\Package\Config"
copy "%base_dir%\Config\Project64.rdx" "%base_dir%\Bin\Package\Config"
copy "%base_dir%\Lang\*.pj.Lang" "%base_dir%\Bin\Package\Lang"
copy "%base_dir%\Plugin\%VSPlatform%\Audio\Jabo_Dsound.dll" "%base_dir%\Bin\Package\Plugin\Audio"
copy "%base_dir%\Plugin\%VSPlatform%\Audio\Project64-Audio.dll" "%base_dir%\Bin\Package\Plugin\Audio"
copy "%base_dir%\Plugin\%VSPlatform%\GFX\Jabo_Direct3D8.dll" "%base_dir%\Bin\Package\Plugin\GFX"
copy "%base_dir%\Plugin\%VSPlatform%\GFX\Project64-Video.dll" "%base_dir%\Bin\Package\Plugin\GFX"
copy "%base_dir%\Plugin\%VSPlatform%\Input\PJ64_NRage.dll" "%base_dir%\Bin\Package\Plugin\Input"
copy "%base_dir%\Plugin\%VSPlatform%\Input\Project64-Input.dll" "%base_dir%\Bin\Package\Plugin\Input"
copy "%base_dir%\Plugin\%VSPlatform%\RSP\Project64-RSP.dll" "%base_dir%\Bin\Package\Plugin\RSP"

cd %base_dir%\Bin\Package
"%zip%" a -tzip -r "%base_dir%\Package\%ZipFileName%" *
cd /d %current_dir%

echo Package %ZipFileName% created

goto :end


:EndErr
ENDLOCAL
echo Build failed
exit /B 1

:End
ENDLOCAL
exit /B 0
