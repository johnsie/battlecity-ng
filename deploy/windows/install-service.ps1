# Installs the Battle City server as a Windows service (run from an elevated PowerShell).
# Needs NSSM (https://nssm.cc) because bc-server.exe is a console program, not a native service.
# Usage: .\install-service.ps1 -Dir C:\BattleCity [-Port 9680] [-Nssm C:\tools\nssm.exe]
param(
  [Parameter(Mandatory = $true)][string]$Dir,    # folder with bc-server.exe and map.dat (unzip the release here)
  [int]$Port = 9680,
  [string]$Nssm = "nssm.exe"
)
$ErrorActionPreference = "Stop"
if (-not (Test-Path "$Dir\bc-server.exe")) { throw "bc-server.exe not found in $Dir" }
New-Item -ItemType Directory -Force "$Dir\logs" | Out-Null

& $Nssm install BattleCity "$Dir\bc-server.exe"
& $Nssm set BattleCity AppDirectory $Dir
& $Nssm set BattleCity AppEnvironmentExtra "BC_PORT=$Port"
& $Nssm set BattleCity Start SERVICE_AUTO_START
& $Nssm set BattleCity AppExit Default Restart          # restart after a crash...
& $Nssm set BattleCity AppRestartDelay 5000             # ...after 5 seconds
& $Nssm set BattleCity AppStdout "$Dir\logs\server.log"
& $Nssm set BattleCity AppStderr "$Dir\logs\server.log"
& $Nssm set BattleCity AppRotateFiles 1
& $Nssm set BattleCity AppRotateBytes 10485760          # rotate at 10 MB

New-NetFirewallRule -DisplayName "Battle City server ($Port/tcp)" -Direction Inbound -Protocol TCP -LocalPort $Port -Action Allow -ErrorAction SilentlyContinue | Out-Null
& $Nssm start BattleCity
Write-Host "Battle City server installed and started on port $Port. Logs: $Dir\logs\server.log"
