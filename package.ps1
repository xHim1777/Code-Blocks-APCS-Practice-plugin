$ErrorActionPreference = 'Stop'
Push-Location -LiteralPath $PSScriptRoot
try {
    [xml]$manifest = Get-Content -LiteralPath 'manifest.xml' -Raw
    $plugin = $manifest.CodeBlocks_plugin_manifest_file.Plugin
    foreach ($attribute in @('title', 'version', 'description', 'author', 'license')) {
        $values = @($plugin.Value | Where-Object { $_.HasAttribute($attribute) })
        if ($values.Count -ne 1 -or [string]::IsNullOrWhiteSpace($values[0].GetAttribute($attribute))) {
            throw "Code::Blocks manifest requires one Value with a nonempty '$attribute' attribute."
        }
    }
    if ($plugin.name -ne 'APCSPracticeStudio') { throw 'Plugin registration name mismatch' }
    Compress-Archive -LiteralPath 'manifest.xml' -DestinationPath 'dist/APCSPracticeStudio.zip' -Force
    Compress-Archive -LiteralPath 'dist/APCSPracticeStudio.dll','dist/APCSPracticeStudio.zip' -DestinationPath 'dist/APCSPracticeStudio-package.zip' -Force
    Copy-Item -LiteralPath 'dist/APCSPracticeStudio-package.zip' -Destination 'dist/APCSPracticeStudio.cbplugin' -Force

    # Verify the manifest inside the final nested package, using the attributes
    # read by PluginManager::ReadManifestFile in the Code::Blocks SDK.
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $outer = [System.IO.Compression.ZipFile]::OpenRead((Join-Path $PSScriptRoot 'dist/APCSPracticeStudio.cbplugin'))
    try {
        $entry = $outer.GetEntry('APCSPracticeStudio.zip')
        if (-not $entry -or -not $outer.GetEntry('APCSPracticeStudio.dll')) { throw 'Package payload missing' }
        $memory = [System.IO.MemoryStream]::new()
        $stream = $entry.Open()
        try { $stream.CopyTo($memory) } finally { $stream.Dispose() }
        $memory.Position = 0
        $inner = [System.IO.Compression.ZipArchive]::new($memory, [System.IO.Compression.ZipArchiveMode]::Read)
        try {
            $reader = [System.IO.StreamReader]::new($inner.GetEntry('manifest.xml').Open())
            try { [xml]$packaged = $reader.ReadToEnd() } finally { $reader.Dispose() }
            foreach ($attribute in @('title', 'version', 'description', 'author', 'license')) {
                $expected = @($plugin.Value | Where-Object { $_.HasAttribute($attribute) })[0].GetAttribute($attribute)
                $actual = @($packaged.CodeBlocks_plugin_manifest_file.Plugin.Value | Where-Object { $_.HasAttribute($attribute) })
                if ($actual.Count -ne 1 -or $actual[0].GetAttribute($attribute) -cne $expected) { throw "Packaged '$attribute' mismatch" }
            }
            $title = @($plugin.Value | Where-Object { $_.HasAttribute('title') })[0].title
            $version = @($plugin.Value | Where-Object { $_.HasAttribute('version') })[0].version
            Write-Output "Verified package: $title / $version"
        } finally { $inner.Dispose(); $memory.Dispose() }
    } finally { $outer.Dispose() }
} finally { Pop-Location }
