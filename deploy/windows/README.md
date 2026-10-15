# Running the Battle City server on Windows

1. Download `battlecity-server-<version>-windows-x64.zip` from the [Releases](https://github.com/johnsie/battlecity-ng/releases) page and unzip it, for example to `C:\BattleCity`. It contains `bc-server.exe`, `bc-server.pdb` and `map.dat`.
2. Try it in a console first: `cd C:\BattleCity` then `bc-server.exe`. It listens on TCP port 9680; set `BC_PORT` to change it. State is written next to the program (`dat\BCData.db`, `logs\`, `news.txt`).
3. To run it as a service that starts with Windows and restarts after a crash, install [NSSM](https://nssm.cc) and run from an elevated PowerShell:

       .\install-service.ps1 -Dir C:\BattleCity -Nssm C:\tools\nssm.exe

   The script also adds the firewall rule for the port. Forward the port on your router if players connect from the internet.
4. If the server ever crashes it prints a stack trace with function names and line numbers. Keep `bc-server.pdb` next to the exe so those are readable, and send the trace with a bug report.

Back up `dat\BCData.db` regularly: it holds every account.

The install script has not been run on a real machine yet.
