# Показати останні N рядків UTF-8 файлу в консолі
param([string]$file, [int]$n = 30)
[Console]::OutputEncoding = [Text.Encoding]::UTF8
Get-Content -Encoding UTF8 -Tail $n $file
