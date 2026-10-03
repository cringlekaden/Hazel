$ErrorActionPreference = 'Stop'
& python (Join-Path $PSScriptRoot 'hazel.py') setup @args
exit $LASTEXITCODE
