param(
    [string]$Action = "status",
    [string]$Ssid = "",
    [string]$Passphrase = ""
)

try {
    Add-Type -AssemblyName System.Runtime.WindowsRuntime
    $asTaskOp = [System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object { $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1' }
    $asTaskAct = [System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object { $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncAction' }

    $profile = [Windows.Networking.Connectivity.NetworkInformation, Windows.Networking.Connectivity, ContentType = WindowsRuntime]::GetInternetConnectionProfile()
    if (-not $profile) {
        Write-Output "ERROR:NO_PROFILE"
        exit 1
    }

    $mgr = [Windows.Networking.NetworkOperators.NetworkOperatorTetheringManager, Windows.Networking.NetworkOperators, ContentType = WindowsRuntime]::CreateFromConnectionProfile($profile)

    if ($Action -eq "start") {
        $cfg = $mgr.GetCurrentAccessPointConfiguration()
        if ($Ssid -and $Ssid.Trim().Length -gt 0) { $cfg.Ssid = $Ssid }
        if ($Passphrase -and $Passphrase.Trim().Length -ge 8) { $cfg.Passphrase = $Passphrase }
        $cfg.Band = [Windows.Networking.NetworkOperators.TetheringWiFiBand]::TwoPointFourGigahertz
        if ($asTaskAct.Count -gt 0) {
            $asTaskAct[0].Invoke($null, @($mgr.ConfigureAccessPointAsync($cfg))).Wait(3000)
        }
        
        $asyncOp = $mgr.StartTetheringAsync()
        $task = $asTaskOp[0].MakeGenericMethod([Windows.Networking.NetworkOperators.NetworkOperatorTetheringOperationResult]).Invoke($null, @($asyncOp))
        $task.Wait(10000)
        $clients = ($mgr.GetTetheringClients() | Measure-Object).Count
        Write-Output "RESULT:$($task.Result.Status)|$($mgr.TetheringOperationalState)|$clients|$($cfg.Ssid)"
    }
    elseif ($Action -eq "stop") {
        $asyncOp = $mgr.StopTetheringAsync()
        $task = $asTaskOp[0].MakeGenericMethod([Windows.Networking.NetworkOperators.NetworkOperatorTetheringOperationResult]).Invoke($null, @($asyncOp))
        $task.Wait(5000)
        Write-Output "RESULT:$($task.Result.Status)|$($mgr.TetheringOperationalState)|0"
    }
    elseif ($Action -eq "status") {
        $clients = ($mgr.GetTetheringClients() | Measure-Object).Count
        $cfg = $mgr.GetCurrentAccessPointConfiguration()
        Write-Output "RESULT:$($mgr.TetheringOperationalState)|$clients|$($cfg.Ssid)"
    }
} catch {
    Write-Output "ERROR:$($_.Exception.Message)"
    exit 1
}
