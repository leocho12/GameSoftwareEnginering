param([string]$Snapshot = "", [int]$Port = 8765)
$ErrorActionPreference = "Stop"
$server = Join-Path $PSScriptRoot "server.py"
$arguments = @($server, "--port", "$Port", "--open")
if ($Snapshot) { $arguments += @("--snapshot", $Snapshot) }
if (Get-Command py -ErrorAction SilentlyContinue) {
    & py -3 @arguments
} elseif (Get-Command python -ErrorAction SilentlyContinue) {
    & python @arguments
} else {
    $bundled = Join-Path $env:USERPROFILE ".cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe"
    if (!(Test-Path -LiteralPath $bundled)) { throw "Python 3이 필요합니다." }
    & $bundled @arguments
}
