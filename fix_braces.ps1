$file = "Source\Planet_Conquest\Core\AITeamController.cpp"
$lines = Get-Content $file

# Fix line 461 - closing brace for line 459 (should have 7 tabs)
if ($lines.Count -gt 460 -and $lines[460] -match '^\t{6}\}$') {
    $lines[460] = "`t`t`t`t`t`t`t}"
    Write-Host "Fixed line 461 closing brace (added 1 tab)"
}

# Fix line 462 - closing brace for line 457 (should have 6 tabs)
if ($lines.Count -gt 461 -and $lines[461] -match '^\t{5}\}$') {
    $lines[461] = "`t`t`t`t`t`t}"
    Write-Host "Fixed line 462 closing brace (added 1 tab)"
}

# Fix line 463 - closing brace for line 455 (should have 5 tabs)
if ($lines.Count -gt 462 -and $lines[462] -match '^\t{4}\}$') {
    $lines[462] = "`t`t`t`t`t}"
    Write-Host "Fixed line 463 closing brace (added 1 tab)"
}

# Fix line 486 - closing brace (vehicle section, should have 7 tabs)
if ($lines.Count -gt 485 -and $lines[485] -match '^\t{6}\}$') {
    $lines[485] = "`t`t`t`t`t`t`t}"
    Write-Host "Fixed line 486 closing brace (added 1 tab)"
}

# Fix line 487 - closing brace (should have 6 tabs)
if ($lines.Count -gt 486 -and $lines[486] -match '^\t{5}\}$') {
    $lines[486] = "`t`t`t`t`t`t}"
    Write-Host "Fixed line 487 closing brace (added 1 tab)"
}

# Fix line 488 - closing brace (should have 5 tabs)
if ($lines.Count -gt 487 -and $lines[487] -match '^\t{4}\}$') {
    $lines[487] = "`t`t`t`t`t}"
    Write-Host "Fixed line 488 closing brace (added 1 tab)"
}

$lines | Set-Content $file
Write-Host "Done"
