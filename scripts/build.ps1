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
    $relativeIntermediatePath = "$relativeOutputPath\build"
    $relativeRuntimePath = "$relativeOutputPath\Savings Jar"
    $intermediatePath = Join-Path $workspaceRoot $relativeIntermediatePath
    $runtimePath = Join-Path $workspaceRoot $relativeRuntimePath
    New-Item -ItemType Directory -Path $intermediatePath -Force | Out-Null
    New-Item -ItemType Directory -Path $runtimePath -Force | Out-Null

    $webViewNativePath = & (Join-Path $PSScriptRoot 'restore-webview2.ps1')
    $resourcePath = Join-Path $relativeIntermediatePath 'AppResources.res'
    $rcArguments = @(
        '/nologo',
        ('/fo' + $resourcePath),
        'data\AppResources.rc'
    )

    $arguments = @(
        '/Zi',
        '/EHsc',
        '/nologo',
        '/std:c++17',
        ('/I' + (Join-Path $webViewNativePath 'include')),
        ('/Fo' + $relativeIntermediatePath + '\'),
        ('/Fd' + $relativeIntermediatePath + '\SavingsJarCompiler.pdb'),
        ('/Fe' + $relativeRuntimePath + '\Savings Jar.exe'),
        'data\MoneySavingWidgetPrototype.cpp',
        'data\MoneySaverWindow.cpp',
        'data\WebViewWindow.cpp',
        'data\DepositButton.cpp',
        'data\SavingsProgress.cpp',
        'data\PresetDepositButtons.cpp',
        'bin\SavingsData.cpp',
        'bin\GoalManager.cpp',
        'bin\GoalLifecycle.cpp',
        'bin\AppPaths.cpp',
        'bin\CloseBehaviorSettings.cpp',
        'bin\StartupSettings.cpp',
        'bin\ReminderSettings.cpp',
        'bin\SingleInstance.cpp',
        'bin\CurrencyRates.cpp',
        '/link',
        ('/LIBPATH:' + (Join-Path $webViewNativePath 'x64')),
        ('/PDB:' + $relativeIntermediatePath + '\SavingsJar.pdb'),
        '/INCREMENTAL:NO',
        $resourcePath,
        'User32.lib',
        'Gdi32.lib',
        'Ole32.lib',
        'Shell32.lib',
        'Dwmapi.lib',
        'Advapi32.lib',
        'version.lib',
        'Winhttp.lib',
        'WebView2LoaderStatic.lib',
        'WindowsApp.lib'
    )

    $logPath = Join-Path $iterationPath 'build.log'
    "Build iteration $nextIteration started at $(Get-Date -Format o)" |
        Set-Content -LiteralPath $logPath

    $rcOutput = & rc.exe @rcArguments 2>&1
    $rcExitCode = $LASTEXITCODE
    $rcOutput | Tee-Object -FilePath $logPath -Append
    if ($rcExitCode -ne 0) {
        "Resource compilation failed with exit code $rcExitCode." |
            Add-Content -LiteralPath $logPath
        Write-Error "Resource compilation failed. See $logPath"
        exit $rcExitCode
    }

    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $compilerOutput = & cl.exe @arguments 2>&1
    $buildExitCode = $LASTEXITCODE
    $ErrorActionPreference = $previousErrorActionPreference
    $compilerOutput | Tee-Object -FilePath $logPath -Append

    if ($buildExitCode -eq 0) {
        $uiPath = Join-Path $runtimePath 'ui'
        $themePath = Join-Path $runtimePath 'Theme'
        New-Item -ItemType Directory -Path $uiPath -Force | Out-Null
        New-Item -ItemType Directory -Path $themePath -Force | Out-Null
        Copy-Item -Path (Join-Path $workspaceRoot 'data\ui\*') -Destination $uiPath -Recurse -Force
        Copy-Item -Path (Join-Path $workspaceRoot 'Theme\*') -Destination $themePath -Recurse -Force
        Remove-Item -LiteralPath (Join-Path $themePath 'README.md') -ErrorAction SilentlyContinue

        $packagePath = Join-Path $iterationPath 'SavingsJar-Windows-x64.zip'
        Compress-Archive -Path (Join-Path $runtimePath '*') -DestinationPath $packagePath -Force

        'Build succeeded.' | Add-Content -LiteralPath $logPath
        Write-Output "Build succeeded: $runtimePath"
        Write-Output "Shareable package: $packagePath"
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