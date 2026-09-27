$ErrorActionPreference = 'Stop'

$workspaceRoot = Split-Path -Parent $PSScriptRoot
$prototypeRoot = Join-Path $workspaceRoot 'prototypes'
New-Item -ItemType Directory -Path $prototypeRoot -Force | Out-Null

$nextIteration = 1
Get-ChildItem -LiteralPath $prototypeRoot -Directory | ForEach-Object {
    if ($_.Name -match '^iteration-(\d+)$') {
        $number = [int]$Matches[1]
        if ($number -ge $nextIteration) {
            $nextIteration = $number + 1
        }
    }
}

$iterationName = "iteration-$nextIteration"
$iterationPath = Join-Path $prototypeRoot $iterationName
New-Item -ItemType Directory -Path $iterationPath | Out-Null

Push-Location $workspaceRoot
try {
    $relativeOutputPath = "prototypes\$iterationName"
    $webViewNativePath = & (Join-Path $PSScriptRoot 'restore-webview2.ps1')
    $arguments = @(
        '/Zi',
        '/EHsc',
        '/nologo',
        '/std:c++17',
        ('/I' + (Join-Path $webViewNativePath 'include')),
        ('/Fo' + $relativeOutputPath + '\'),
        ('/Fd' + $relativeOutputPath + '\MoneySavingWidgetCompiler.pdb'),
        ('/Fe' + $relativeOutputPath + '\MoneySavingWidget.exe'),
        'data\MoneySavingWidgetPrototype.cpp',
        'data\MoneySaverWindow.cpp',
        'data\WebViewWindow.cpp',
        'data\DepositButton.cpp',
        'data\SavingsProgress.cpp',
        'data\PresetDepositButtons.cpp',
        'bin\SavingsData.cpp',
        'bin\CurrencyRates.cpp',
        '/link',
        ('/LIBPATH:' + (Join-Path $webViewNativePath 'x64')),
        'User32.lib',
        'Gdi32.lib',
        'Ole32.lib',
        'version.lib',
        'Winhttp.lib',
        'WebView2LoaderStatic.lib',
        'WindowsApp.lib'
    )

    $logPath = Join-Path $iterationPath 'build.log'
    "Build iteration $nextIteration started at $(Get-Date -Format o)" |
        Set-Content -LiteralPath $logPath

    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $compilerOutput = & cl.exe @arguments 2>&1
    $buildExitCode = $LASTEXITCODE
    $ErrorActionPreference = $previousErrorActionPreference
    $compilerOutput | Tee-Object -FilePath $logPath -Append

    if ($buildExitCode -eq 0) {
        'Build succeeded.' | Add-Content -LiteralPath $logPath
        Write-Output "Build succeeded: $iterationPath"
    }
    else {
        "Build failed with exit code $buildExitCode." |
            Add-Content -LiteralPath $logPath
        Write-Error "Build failed. See $logPath"
    }
}
finally {
    Pop-Location
}

exit $buildExitCode