$ErrorActionPreference = 'Stop'

$version = '1.0.4191.47'
$workspaceRoot = Split-Path -Parent $PSScriptRoot
$packageRoot = Join-Path $workspaceRoot ".packages\Microsoft.Web.WebView2\$version"
$nativeRoot = Join-Path $packageRoot 'build\native'
$headerPath = Join-Path $nativeRoot 'include\WebView2.h'

if (-not (Test-Path -LiteralPath $headerPath)) {
    if (Test-Path -LiteralPath $packageRoot) {
        Remove-Item -LiteralPath $packageRoot -Recurse -Force
    }

    New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null
    $packagePath = Join-Path $packageRoot "Microsoft.Web.WebView2.$version.nupkg"
    $packageUrl = "https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/$version/microsoft.web.webview2.$version.nupkg"
    Invoke-WebRequest -Uri $packageUrl -OutFile $packagePath -UseBasicParsing

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [System.IO.Compression.ZipFile]::ExtractToDirectory(
        $packagePath, (Join-Path $packageRoot 'package'))
    $extractedNativeRoot = Join-Path $packageRoot 'package\build\native'
    $nativeRoot = $extractedNativeRoot
    $headerPath = Join-Path $nativeRoot 'include\WebView2.h'
}

if (-not (Test-Path -LiteralPath $headerPath)) {
    throw "WebView2 SDK $version did not contain its native headers."
}

Write-Output $nativeRoot