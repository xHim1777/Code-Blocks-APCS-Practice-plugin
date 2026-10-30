param(
    [string]$CodeBlocks = 'C:\Program Files\CodeBlocks',
    [string]$Dependencies = (Join-Path $PSScriptRoot '..\apcs-plugin')
)
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
$gpp = Join-Path $CodeBlocks 'MinGW\bin\g++.exe'
$windres = Join-Path $CodeBlocks 'MinGW\bin\windres.exe'
$outDir = Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Path $outDir, 'dist' -Force | Out-Null
function Run([string]$exe, [string[]]$arguments) {
    & $exe @arguments
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $exe ($LASTEXITCODE)" }
}
$common = @('-std=c++17','-O2','-Wall','-Wextra','-static','-municode')
Run $gpp ($common + @('src/build_helper.cpp','-lbcrypt','-o','build/PracticeBuild.exe'))
Run $gpp ($common + @('tests/core_tests.cpp','-o','build/core_tests.exe'))
Run (Join-Path $outDir 'core_tests.exe') @()
$helper = (Join-Path $outDir 'PracticeBuild.exe').Replace('\','/')
Set-Content -LiteralPath 'build/resource.rc' -Value "101 RCDATA `"$helper`"" -Encoding ascii
Run $windres @('build/resource.rc','-O','coff','-o','build/resource.o')
$sdk = Join-Path $Dependencies 'sdk-source\codeblocks_25.03\src'
$includes = @("-I$Dependencies/include", "-I$Dependencies/lib/gcc_dll/mswu", "-I$sdk/include", "-I$sdk/include/tinyxml", "-I$sdk/sdk/wxscintilla/include")
Run $gpp (@('-std=c++17','-O2','-Wall','-Wextra','-DBUILDING_PLUGIN','-D__WXMSW__','-DWXUSINGDLL','-D_UNICODE') + $includes + @('-c','src/plugin.cpp','-o','build/plugin.o'))
$nm = Join-Path $CodeBlocks 'MinGW\bin\nm.exe'
$gendef = Join-Path $CodeBlocks 'MinGW\bin\gendef.exe'
$dlltool = Join-Path $CodeBlocks 'MinGW\bin\dlltool.exe'
$undefined = @{}
& $nm -u build/plugin.o | ForEach-Object {
    if ($_ -match '\sU\s+(\S+)$') { $undefined[$Matches[1] -replace '^__imp_', ''] = $true }
}
Push-Location -LiteralPath $outDir
try {
    foreach ($dll in @('codeblocks','wxmsw32u_gcc_custom')) {
        Run $gendef @((Join-Path $CodeBlocks "$dll.dll"))
        $symbols = @("LIBRARY $dll.dll", 'EXPORTS')
        Get-Content -LiteralPath "$dll.def" | ForEach-Object {
            $sym = ($_ -split '\s+')[0]
            if ($undefined.ContainsKey($sym)) {
                $data = $sym -match '^_ZT[VSI]|^wxEVT_|^cbEVT_|^wxEmptyString$|NameStr$|CaptionStr$|^wxTheAssertHandler$|^wxTrapInAssert$|^_ZN.*(sm_|ms_)'
                $symbols += $sym + $(if ($data) { ' DATA' } else { '' })
            }
        }
        Set-Content -LiteralPath "$dll-import.def" -Value $symbols -Encoding ascii
        Run $dlltool @('-d',"$dll-import.def",'-l',"lib$dll.a",'-D',"$dll.dll")
    }
} finally { Pop-Location }
Run $gpp @('-shared','-static','build/plugin.o','build/resource.o','-Lbuild','-lcodeblocks','-lwxmsw32u_gcc_custom','-o','dist/APCSPracticeStudio.dll')
& (Join-Path $PSScriptRoot 'package.ps1')
