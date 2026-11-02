[CmdletBinding()]
param
(
    [ValidateSet('Check', 'Run')]
    [string] $Action = 'Check',
    [string] $Script,
    [string[]] $ScriptArguments = @()
)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$settingsPath = Join-Path $PSScriptRoot 'local.settings.json'
$blenderExecutable = $env:BLENDER_EXECUTABLE

if ([string]::IsNullOrWhiteSpace($blenderExecutable) -and (Test-Path -LiteralPath $settingsPath))
{
    $settings = Get-Content -LiteralPath $settingsPath -Raw | ConvertFrom-Json
    $blenderExecutable = $settings.executablePath
}

if ([string]::IsNullOrWhiteSpace($blenderExecutable))
{
    $command = Get-Command blender -ErrorAction SilentlyContinue
    if ($command)
    {
        $blenderExecutable = $command.Source
    }
}

if ([string]::IsNullOrWhiteSpace($blenderExecutable) -or
    -not (Test-Path -LiteralPath $blenderExecutable -PathType Leaf))
{
    throw 'Blender executable not found. Set BLENDER_EXECUTABLE or tools/blender/local.settings.json.'
}

if ($Action -eq 'Check')
{
    $scriptPath = Join-Path $PSScriptRoot 'probe_blender.py'
    $scriptParameters = @('--output', (Join-Path $PSScriptRoot 'connection.local.json'))
}
else
{
    if ([string]::IsNullOrWhiteSpace($Script))
    {
        throw '-Script is required when using -Action Run.'
    }
    $scriptPath = if ([IO.Path]::IsPathRooted($Script))
    {
        $Script
    }
    else
    {
        Join-Path $projectRoot $Script
    }
    if (-not (Test-Path -LiteralPath $scriptPath -PathType Leaf))
    {
        throw "Blender Python script not found: $scriptPath"
    }
    $scriptPath = (Resolve-Path -LiteralPath $scriptPath).Path
    $scriptParameters = $ScriptArguments
}

$blenderArguments = @(
    '--background',
    '--factory-startup',
    '--python-exit-code', '1',
    '--python', $scriptPath,
    '--'
) + $scriptParameters

Push-Location -LiteralPath $projectRoot
try
{
    & $blenderExecutable @blenderArguments
    if ($LASTEXITCODE -ne 0)
    {
        throw "Blender failed with exit code $LASTEXITCODE."
    }
}
finally
{
    Pop-Location
}
