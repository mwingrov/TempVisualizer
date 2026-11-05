param(
    [string]$Path = (Join-Path $PSScriptRoot "live_test_log.txt"),
    [ValidateRange(1, 3600)]
    [int]$IntervalSeconds = 1,
    [ValidateRange(0, 1000000)]
    [int]$Count = 0
)

$resolvedPath = [System.IO.Path]::GetFullPath($Path)
$temperatures = [ordered]@{
    TempR2   = 20
    TempR3   = 19
    TempR4   = 20
    TempR5LF = 19
    TempR5LM = 19
    TempR5LB = 20
    TempR5RF = 20
    TempR5RM = 19
    TempR5RB = 19
}

$stream = [System.IO.FileStream]::new(
    $resolvedPath,
    [System.IO.FileMode]::Append,
    [System.IO.FileAccess]::Write,
    [System.IO.FileShare]::ReadWrite
)
$writer = [System.IO.StreamWriter]::new(
    $stream,
    [System.Text.UTF8Encoding]::new($false)
)

Write-Host "Appending live test data to $resolvedPath"
if ($Count -eq 0) {
    Write-Host "Press Ctrl+C to stop."
}

try {
    $recordsWritten = 0
    while ($Count -eq 0 -or $recordsWritten -lt $Count) {
        foreach ($key in @($temperatures.Keys)) {
            $temperatures[$key] = [Math]::Max(
                15,
                [Math]::Min(25, $temperatures[$key] + (Get-Random -Minimum -2 -Maximum 3))
            )
        }

        $now = Get-Date
        $record = [ordered]@{}
        foreach ($key in $temperatures.Keys) {
            $record[$key] = $temperatures[$key]
        }
        $record["Date"] = $now.ToString("yyyy-MM-dd")
        $record["Time"] = $now.ToString("HH:mm:ss")

        $json = ConvertTo-Json -InputObject $record -Compress
        $line = '{0} | Data | {1}' -f $now.ToString("yyyy-MM-dd HH:mm:ss"), $json
        $writer.WriteLine($line)
        $writer.Flush()
        $recordsWritten++
        Write-Host $line

        if ($Count -eq 0 -or $recordsWritten -lt $Count) {
            Start-Sleep -Seconds $IntervalSeconds
        }
    }
}
finally {
    $writer.Dispose()
}
