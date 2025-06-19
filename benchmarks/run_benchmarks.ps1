# PowerShell script to run all Tocin benchmarks and report results
$benchmarks = Get-ChildItem -Path . -Filter *.to
$compiler = "../build/tocin.exe"

if (!(Test-Path $compiler)) {
    Write-Host "Compiler not found: $compiler"
    exit 1
}

foreach ($bench in $benchmarks) {
    Write-Host "Running benchmark: $($bench.Name)"
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    & $compiler $bench.FullName --jit
    $sw.Stop()
    Write-Host "Time: $($sw.Elapsed.TotalSeconds) seconds"
    Write-Host "---"
} 