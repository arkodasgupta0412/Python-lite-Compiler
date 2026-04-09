$ErrorActionPreference = 'Continue'
New-Item -ItemType Directory -Path .\docs\symbol_tables_AtoG -Force | Out-Null
$tests = 'testA','testB','testC','testD','testE','testF','testG'
$summary = @()
$parseFindings = @()

foreach ($t in $tests) {
    $inputPath = ".\examples\$t.cd"
    $logPath = ".\docs\symbol_tables_AtoG\$t.run.log"
    $srcSt = ".\docs\symbol_table.txt"
    $dstSt = ".\docs\symbol_tables_AtoG\$t.symbol_table.txt"

    if (Test-Path $logPath) { Remove-Item $logPath -Force }

    & .\cd_frontend.exe $inputPath *> $logPath
    $exitCode = $LASTEXITCODE

    $outText = if (Test-Path $logPath) { Get-Content $logPath -Raw } else { '' }
    $hasAccept = $outText -match '(?m)\bACCEPT\b'
    $hasReject = $outText -match '(?m)\bREJECT\b'
    $acceptedFlag = if ($hasAccept -and $hasReject) { 'BOTH' } elseif ($hasAccept) { 'ACCEPT' } elseif ($hasReject) { 'REJECT' } else { 'NONE' }

    if (Test-Path $srcSt) { Copy-Item $srcSt $dstSt -Force }
    $stExists = Test-Path $dstSt
    $stBytes = if ($stExists) { (Get-Item $dstSt).Length } else { 0 }

    $summary += [PSCustomObject]@{
        test = $t
        exitCode = $exitCode
        acceptedFlag = $acceptedFlag
        symbolTableExists = $stExists
        symbolTableBytes = $stBytes
    }

    if ($stExists) {
        $lines = Get-Content $dstSt
        $scopeOrder = New-Object System.Collections.Generic.List[string]
        $scopeRows = @{}
        $scopeCounts = @{}
        $currentScope = $null
        $rowMode = $false

        foreach ($line in $lines) {
            if ($line -match '^\s*===\s*(.+?)\s*===\s*$') {
                $base = $matches[1].Trim()
                if (-not $scopeCounts.ContainsKey($base)) { $scopeCounts[$base] = 0 }
                $scopeCounts[$base]++
                $currentScope = if ($scopeCounts[$base] -gt 1) { "$base#$($scopeCounts[$base])" } else { $base }
                $scopeOrder.Add($currentScope)
                $scopeRows[$currentScope] = New-Object System.Collections.Generic.List[object]
                $rowMode = $false
                continue
            }
            if ($line -match '^\s*Name\s+Type\s+Kind\s+Line\s+Col\s+Offset\s*$') {
                $rowMode = $true
                continue
            }
            if ($line -match '^\s*-{3,}\s*$') { continue }

            if ($rowMode -and $currentScope -and $line.Trim().Length -gt 0) {
                $m = [regex]::Match($line, '^\s*(\S+)\s+(.+?)\s+(\S+)\s+(-?\d+)\s+(-?\d+)\s+(-?\d+)\s*$')
                if ($m.Success) {
                    $scopeRows[$currentScope].Add([PSCustomObject]@{
                        scope = $currentScope
                        name = $m.Groups[1].Value
                        type = $m.Groups[2].Value.Trim()
                        kind = $m.Groups[3].Value
                        line = [int]$m.Groups[4].Value
                        col = [int]$m.Groups[5].Value
                        offset = [int]$m.Groups[6].Value
                    }) | Out-Null
                }
            }
        }

        $rowsFlat = @()
        foreach ($s in $scopeOrder) {
            if ($scopeRows.ContainsKey($s)) { $rowsFlat += $scopeRows[$s] }
        }

        $anomalies = New-Object System.Collections.Generic.List[string]
        foreach ($s in $scopeOrder) {
            $rows = @($scopeRows[$s])
            if ($rows.Count -eq 0) { continue }

            $dupes = $rows | Group-Object name | Where-Object { $_.Count -gt 1 }
            foreach ($d in $dupes) {
                $anomalies.Add("duplicate-in-scope $s : $($d.Name) x$($d.Count)") | Out-Null
            }

            $badNeg = $rows | Where-Object { $_.offset -lt 0 -and $_.offset -ne -1 }
            foreach ($bn in $badNeg) {
                $anomalies.Add("negative-offset $s : $($bn.name) offset=$($bn.offset)") | Out-Null
            }

            $prev = $null
            foreach ($r in $rows) {
                if ($r.offset -eq -1) { continue }
                if ($null -ne $prev -and $r.offset -lt $prev) {
                    $anomalies.Add("non-monotonic-offset $s : $($r.name) offset=$($r.offset) after $prev") | Out-Null
                }
                $prev = $r.offset
            }
        }

        $parseFindings += [PSCustomObject]@{
            test = $t
            scopeHeaders = ($scopeOrder -join ' -> ')
            rows = $rowsFlat
            anomalies = @($anomalies)
        }
    }
    else {
        $parseFindings += [PSCustomObject]@{
            test = $t
            scopeHeaders = ''
            rows = @()
            anomalies = @('symbol table missing')
        }
    }
}

"SUMMARY_TABLE"
$summary | Format-Table -AutoSize | Out-String
"PARSE_FINDINGS"
foreach ($pf in $parseFindings) {
    "TEST: $($pf.test)"
    "SCOPE_HEADERS: $($pf.scopeHeaders)"
    "ROWS:"
    if ($pf.rows.Count -gt 0) {
        $pf.rows | ForEach-Object { "  $($_.scope) | $($_.name) | $($_.type) | $($_.kind) | $($_.line) | $($_.col) | $($_.offset)" }
    }
    else {
        "  (none)"
    }

    "ANOMALIES:"
    if ($pf.anomalies.Count -gt 0) {
        $pf.anomalies | ForEach-Object { "  $_" }
    }
    else {
        "  (none)"
    }
    ""
}

$summary | ConvertTo-Json -Depth 5 | Set-Content .\docs\symbol_tables_AtoG\summary.json
$parseFindings | ConvertTo-Json -Depth 8 | Set-Content .\docs\symbol_tables_AtoG\parse_findings.json
