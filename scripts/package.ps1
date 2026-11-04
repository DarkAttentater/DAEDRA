# Builds dist\DAEDRA-<version>[-rN].zip from dist-template plus the built DLL.
# -Test adds tests\ingame\*.json (throw-away rules for in-game checks) and turns logMatches on.
param([string]$Revision = "", [switch]$Test)
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$version = "0.1.0"
$name = "DAEDRA-$version" + $(if ($Revision) { "-r$Revision" } else { "" })
$stage = Join-Path $root "dist\$name"
# Only the newest revision is kept in dist; older builds are removed.
if (Test-Path (Join-Path $root "dist")) { Get-ChildItem (Join-Path $root "dist") | Remove-Item -Recurse -Force }
New-Item -ItemType Directory -Path $stage | Out-Null
Copy-Item (Join-Path $root "dist-template\SKSE") $stage -Recurse
Copy-Item (Join-Path $root "build\plugin\Release\DAEDRA.dll") (Join-Path $stage "SKSE\Plugins")
if ($Test) {
    Copy-Item (Join-Path $root "tests\ingame\*.json") (Join-Path $stage "SKSE\Plugins\DAEDRA")
    Copy-Item (Join-Path $root "tests\ingame\PNGIconInjector") (Join-Path $stage "SKSE\Plugins") -Recurse
    $ini = Join-Path $stage "SKSE\Plugins\DAEDRA.ini"
    (Get-Content $ini) -replace '^logMatches = false', 'logMatches = true' | Set-Content $ini
}
$zip = Join-Path $root "dist\$name.zip"
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path (Join-Path $stage "SKSE") -DestinationPath $zip
Write-Output $zip
