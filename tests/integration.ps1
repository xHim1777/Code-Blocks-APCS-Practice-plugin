$ErrorActionPreference = 'Stop'
$pluginRoot = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $pluginRoot
$fixtureRoot = [System.IO.File]::ReadAllText((Join-Path $pluginRoot 'build\last-test-root.txt'))
$settingsPath = Join-Path $fixtureRoot 'setting.json'
$settings = Get-Content -LiteralPath $settingsPath -Raw -Encoding UTF8 | ConvertFrom-Json
$settings.compiler = 'C:\Program Files\CodeBlocks\MinGW\bin\g++.exe'
[System.IO.File]::WriteAllText($settingsPath, ($settings | ConvertTo-Json), [System.Text.UTF8Encoding]::new($false))
$practiceName = '復原題目'
$cpp = Join-Path $fixtureRoot "allPractice\$practiceName\$practiceName.cpp"
$helper = Join-Path $pluginRoot 'build\PracticeBuild.exe'
$valid = "#include <iostream>`nint main() { std::cout << 42 << '\n'; return 0; }`n"
[System.IO.File]::WriteAllText($cpp, $valid, [System.Text.UTF8Encoding]::new($false))
& $helper build $fixtureRoot $practiceName
if ($LASTEXITCODE -ne 0) { throw 'Valid C++ build failed' }
$exe = Join-Path $fixtureRoot "allPractice\$practiceName\$practiceName.exe"
$actual = & $exe
if ($LASTEXITCODE -ne 0 -or $actual -ne '42') { throw 'Executable output mismatch' }
if (-not (Test-Path -LiteralPath (Join-Path $fixtureRoot "allPractice\$practiceName\$practiceName.o"))) { throw 'Object file missing' }
$invalid = "int main() { invalid token; }`n"
[System.IO.File]::WriteAllText($cpp, $invalid, [System.Text.UTF8Encoding]::new($false))
& $helper build $fixtureRoot $practiceName
if ($LASTEXITCODE -eq 0) { throw 'Invalid C++ unexpectedly built' }
$logs = @(Get-ChildItem -LiteralPath (Join-Path $fixtureRoot "allLogs\$practiceName") -Filter '*.html' | Where-Object Name -ne 'example.html' | Sort-Object Name)
if ($logs.Count -ne 2) { throw 'Expected separate logs for both builds' }
$success = [System.IO.File]::ReadAllText($logs[0].FullName)
$failure = [System.IO.File]::ReadAllText($logs[1].FullName)
if ($success -notmatch 'Result: SUCCESS' -or $success -notmatch '\+08:00') { throw 'Success/timezone missing' }
if ($failure -notmatch 'Result: FAILED' -or $failure -notmatch 'invalid token') { throw 'Failure snapshot missing' }
$snapshot = [System.Net.WebUtility]::HtmlDecode([regex]::Matches($success, '(?s)<pre>(.*?)</pre>')[1].Groups[1].Value)
if ($snapshot -cne $valid) { throw 'Source snapshot differs from compiled source' }
$before = $logs.Count
& $helper build $fixtureRoot $practiceName
if ($LASTEXITCODE -eq 0) { throw 'Repeated invalid build unexpectedly succeeded' }
$after = @(Get-ChildItem -LiteralPath (Join-Path $fixtureRoot "allLogs\$practiceName") -Filter '*.html' | Where-Object Name -ne 'example.html').Count
if ($after -ne $before + 1) { throw 'Repeated source build not logged' }
& $helper clean $fixtureRoot $practiceName
if ($LASTEXITCODE -ne 0 -or (Test-Path -LiteralPath $exe)) { throw 'Clean failed' }
if (-not (Test-Path -LiteralPath $cpp)) { throw 'Clean removed source' }
Write-Output 'PASS: valid compile/run (42), object/exe paths, failed compile, exact snapshot, Taipei timestamp, repeated-build logging, clean preserves source.'
