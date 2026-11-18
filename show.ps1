# Показати перші або останні N рядків UTF-8 файлу в консолі
# Використання: show.ps1 <файл> [N] [head]
param([string]$file, [int]$n = 30, [string]$mode = "tail")
[Console]::OutputEncoding = [Text.Encoding]::UTF8
if ($mode -eq "head") { Get-Content -Encoding UTF8 -TotalCount $n $file }
else { Get-Content -Encoding UTF8 -Tail $n $file }
