#requires -Version 7.0
param([Parameter(Mandatory=$true)][string]$Gcc,[Parameter(Mandatory=$true)][string]$MinHookSource,[Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference = 'Stop'
# MinHook v1.3.4: c3fcafdc10146beb5919319d0683e44e3c30d537.
# Use a new output path and an ASCII build path if your MinGW linker requires it.
if (-not (Test-Path -LiteralPath $Gcc -PathType Leaf)) { throw 'GCC executable not found' }
if (Test-Path -LiteralPath $Output) { throw 'Output already exists; choose a new path' }
$arguments = @('-shared','-O2','-std=c11','-Wall','-Wextra','-Werror','-static-libgcc','-I',(Join-Path $MinHookSource 'include'),(Join-Path $PSScriptRoot 'runtime.c'),(Join-Path $MinHookSource 'src\buffer.c'),(Join-Path $MinHookSource 'src\hook.c'),(Join-Path $MinHookSource 'src\trampoline.c'),(Join-Path $MinHookSource 'src\hde\hde64.c'),'-lbcrypt','-Wl,--dynamicbase,--nxcompat','-o',$Output)
& $Gcc @arguments
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
Get-FileHash -LiteralPath $Output -Algorithm SHA256
