# Register-EventLog helper for HerculesAC
# Run once as Administrator on a dev/staging machine.

if (-not (Get-WinEvent -ListProvider HerculesAC -ErrorAction SilentlyContinue)) {
	New-EventLog -LogName Application -Source HerculesAC
	Write-Host "Registered Application/HerculesAC event source."
} else {
	Write-Host "HerculesAC event source already registered."
}
