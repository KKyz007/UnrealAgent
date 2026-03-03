$tcpClient = New-Object System.Net.Sockets.TcpClient
try {
    $tcpClient.Connect('127.0.0.1', 55557)
    Write-Host "TCP Connected to 127.0.0.1:55557"

    $stream = $tcpClient.GetStream()
    $encoding = [System.Text.Encoding]::UTF8

    # Helper function to send JSON-RPC and read response
    function Send-Request($method, $params, $id) {
        $payload = '{"jsonrpc":"2.0","method":"' + $method + '","params":' + $params + ',"id":' + $id + '}'
        $payloadBytes = $encoding.GetBytes($payload)
        $header = "Content-Length: " + $payloadBytes.Length + "`r`n`r`n"
        $headerBytes = $encoding.GetBytes($header)

        $stream.Write($headerBytes, 0, $headerBytes.Length)
        $stream.Write($payloadBytes, 0, $payloadBytes.Length)
        $stream.Flush()

        # Read response header
        Start-Sleep -Milliseconds 500
        $buffer = New-Object byte[] 65536
        $bytesRead = $stream.Read($buffer, 0, $buffer.Length)
        $response = $encoding.GetString($buffer, 0, $bytesRead)

        Write-Host ""
        Write-Host "=== $method ==="
        # Extract JSON body after headers
        $parts = $response -split "`r`n`r`n", 2
        if ($parts.Length -gt 1) {
            Write-Host $parts[1]
        } else {
            Write-Host $response
        }
    }

    Send-Request "list_tools" "{}" 1
    Send-Request "get_project_info" "{}" 2
    Send-Request "get_editor_state" "{}" 3

    $tcpClient.Close()
    Write-Host ""
    Write-Host "All tests passed!"
} catch {
    Write-Host ("ERROR: " + $_.Exception.Message)
}
