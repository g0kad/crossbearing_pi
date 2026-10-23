# Local Windows (Win32) build without buildwin\win_deps.bat, which needs
# Chocolatey and admin rights and changes the user PATH permanently.
#
# Needs Visual Studio 2022 (Build Tools are enough) with the C++ workload.
# wxWidgets 3.2.6 and portable gettext are downloaded once into cache\
# (git-ignored); environment changes last only for this script.
#
# Usage, from the repository root:  powershell -File scripts\win-build.ps1
# Output: build-win\CrossBearing-*_msvc-wx32-10-x86.tar.gz, which can be
# installed with OpenCPN's Options > Plugins > Import plugin...

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
$cache = Join-Path $root "cache"
$wx = Join-Path $cache "wxWidgets"
$gettext = Join-Path $cache "gettext"
New-Item -ItemType Directory -Force (Join-Path $cache "dl") | Out-Null

if (-not (Test-Path (Join-Path $wx "lib\vc_dll"))) {
  $base = "https://github.com/wxWidgets/wxWidgets/releases/download/v3.2.6"
  New-Item -ItemType Directory -Force $wx | Out-Null
  foreach ($f in "wxWidgets-3.2.6-headers.7z", "wxMSW-3.2.6_vc14x_ReleaseDLL.7z", "wxMSW-3.2.6_vc14x_Dev.7z") {
    $dest = Join-Path $cache "dl\$f"
    Invoke-WebRequest "$base/$f" -OutFile $dest
    & "$env:SystemRoot\System32\tar.exe" -xf $dest -C $wx
  }
  Rename-Item (Join-Path $wx "lib\vc14x_dll") "vc_dll"
}

if (-not (Test-Path (Join-Path $gettext "bin\msgfmt.exe"))) {
  $zip = Join-Path $cache "dl\gettext.zip"
  Invoke-WebRequest "https://github.com/mlocati/gettext-iconv-windows/releases/download/v1.0-v1.19/gettext1.0-iconv1.19-static-64.zip" -OutFile $zip
  Expand-Archive $zip -DestinationPath $gettext -Force
}

# Use Visual Studio's own CMake: the template's nested build calls "cmake"
# by name, and other CMakes on PATH may be too new.
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
$cmakebin = Join-Path $vs "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
$env:PATH = "$cmakebin;$gettext\bin;$wx\lib\vc_dll;$env:PATH"
$env:WXWIN = $wx
$env:wxWidgets_ROOT_DIR = $wx
$env:wxWidgets_LIB_DIR = "$wx\lib\vc_dll"

if (Test-Path build-win) { Remove-Item -Recurse -Force build-win }
cmake -S . -B build-win -G "Visual Studio 17 2022" -A Win32 -DCMAKE_BUILD_TYPE=RelWithDebInfo `
  "-DwxWidgets_ROOT_DIR=$wx" "-DwxWidgets_LIB_DIR=$wx\lib\vc_dll" "-DOCPN_TARGET_TUPLE=msvc-wx32;10;x86"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build build-win --target tarball --config RelWithDebInfo
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Get-ChildItem build-win\*.tar.gz | ForEach-Object { "Built: $($_.FullName)" }
