# WebServ Integration Test Script
# Run this script while the webserv.exe is running on localhost:8080.

$baseUrl = "http://127.0.0.1:8080"

Write-Host "==================================================" -ForegroundColor Cyan
Write-Host "Starting WebServ Integration Tests" -ForegroundColor Cyan
Write-Host "Target Base URL: $baseUrl" -ForegroundColor Cyan
Write-Host "==================================================" -ForegroundColor Cyan

# Helper function to execute test cases
function Run-TestCase($name, $method, $path, $body, $expectedStatus, $checkBodyMatch) {
    Write-Host "`n[TEST] $name..." -NoNewline
    
    try {
        $headers = @{}
        $requestParams = @{
            Uri = "$baseUrl$path"
            Method = $method
            Headers = $headers
            SkipHttpErrorHandl = $true
            UseBasicParsing = $true
        }
        
        if ($body) {
            $requestParams["Body"] = $body
            $headers["Content-Type"] = "text/plain"
        }

        # For HEAD requests, Invoke-WebRequest behaves normally but doesn't return Content on some PowerShell versions.
        # We manually process response structures.
        $response = Invoke-WebRequest @requestParams
        $statusCode = $response.StatusCode
        
        # Verify status code
        if ($statusCode -eq $expectedStatus) {
            # Optionally check body content
            if ($checkBodyMatch) {
                if ($response.Content -match $checkBodyMatch) {
                    Write-Host " PASS" -ForegroundColor Green
                } else {
                    Write-Host " FAIL (Body did not match '$checkBodyMatch'. Got: '$($response.Content)')" -ForegroundColor Red
                }
            } else {
                Write-Host " PASS" -ForegroundColor Green
            }
        } else {
            Write-Host " FAIL (Expected $expectedStatus, got $statusCode)" -ForegroundColor Red
        }
    } catch {
        Write-Host " FAIL (Connection Error: $_)" -ForegroundColor Red
    }
}

# 1. Test Static File Serving
Run-TestCase "Serve index.html (GET /)" "GET" "/" $null 200 "WebServ Online"
Run-TestCase "Serve style.css (GET /style.css)" "GET" "/style.css" $null 200 "card-bg"
Run-TestCase "Serve 404 page (GET /notfound.html)" "GET" "/notfound.html" $null 404 "Page Not Found"

# 2. Test Dynamic Routing
Run-TestCase "GET health status (/api/health)" "GET" "/api/health" $null 200 "healthy"
Run-TestCase "POST echo payload (/api/echo)" "POST" "/api/echo" "Hello Systems World!" 200 "Echo received: Hello Systems World!"
Run-TestCase "POST echo empty payload" "POST" "/api/echo" $null 400 "Missing body content"

# 3. Test HTTP HEAD request (Headers only, no body content)
Write-Host "`n[TEST] HEAD /api/health (Checking headers only)..." -NoNewline
try {
    $tcp = New-Object System.Net.Sockets.TcpClient("127.0.0.1", 8080)
    $stream = $tcp.GetStream()
    $writer = New-Object System.IO.StreamWriter($stream)
    $reader = New-Object System.IO.StreamReader($stream)

    # Write raw HEAD request to TCP stream
    $writer.WriteLine("HEAD /api/health HTTP/1.1")
    $writer.WriteLine("Host: 127.0.0.1:8080")
    $writer.WriteLine("Connection: close")
    $writer.WriteLine("")
    $writer.Flush()

    # Read response
    $headersOnly = $true
    $hasBody = $false
    while (($line = $reader.ReadLine()) -ne $null) {
        if ($line -eq "") {
            # Blank line signifies end of headers. Anything after is the body.
            $headersOnly = $false
            continue;
        }
        if (-not $headersOnly) {
            # We received content after the blank line!
            $hasBody = $true
        }
    }
    
    $tcp.Close()
    
    if (-not $hasBody) {
        Write-Host " PASS" -ForegroundColor Green
    } else {
        Write-Host " FAIL (HEAD response contained a body!)" -ForegroundColor Red
    }
} catch {
    Write-Host " FAIL (Error: $_)" -ForegroundColor Red
}

# 4. Test Security: Path Traversal Protection
Run-TestCase "Path traversal protection (GET /../src/main.cpp)" "GET" "/../src/main.cpp" $null 400 "Invalid Path"

# 5. Test Robustness: Unsupported HTTP Method
Run-TestCase "Unsupported method check (PUT /index.html)" "PUT" "/index.html" $null 405 $null

# 6. Test Robustness: Malformed HTTP request line
Write-Host "`n[TEST] Malformed request line..." -NoNewline
try {
    $tcp = New-Object System.Net.Sockets.TcpClient("127.0.0.1", 8080)
    $stream = $tcp.GetStream()
    $writer = New-Object System.IO.StreamWriter($stream)
    $reader = New-Object System.IO.StreamReader($stream)

    # Send malformed request line
    $writer.WriteLine("BADREQUEST")
    $writer.WriteLine("")
    $writer.Flush()

    $respLine = $reader.ReadLine()
    $tcp.Close()

    if ($respLine -match "400 Bad Request") {
        Write-Host " PASS" -ForegroundColor Green
    } else {
        Write-Host " FAIL (Expected 400 Bad Request, got: '$respLine')" -ForegroundColor Red
    }
} catch {
    Write-Host " FAIL (Error: $_)" -ForegroundColor Red
}

# 7. Test Robustness: Timeout check (holding connection open without sending data)
Write-Host "`n[TEST] Idle connection timeout (will block for ~10 seconds)..." -NoNewline
try {
    $startTime = Get-Date
    $tcp = New-Object System.Net.Sockets.TcpClient("127.0.0.1", 8080)
    $stream = $tcp.GetStream()
    $reader = New-Object System.IO.StreamReader($stream)

    # Do not write anything! Wait for server to kick us out.
    $respLine = $reader.ReadLine()
    $endTime = Get-Date
    $duration = ($endTime - $startTime).TotalSeconds
    
    $tcp.Close()

    if ($respLine -match "408 Request Timeout" -and $duration -ge 8 -and $duration -le 13) {
        Write-Host " PASS (Timed out in $($duration)s with 408)" -ForegroundColor Green
    } else {
        Write-Host " FAIL (Expected 408 Timeout in ~10s. Got: '$respLine' in $($duration)s)" -ForegroundColor Red
    }
} catch {
    Write-Host " FAIL (Error: $_)" -ForegroundColor Red
}

Write-Host "`n==================================================" -ForegroundColor Cyan
Write-Host "Tests Completed" -ForegroundColor Cyan
Write-Host "==================================================" -ForegroundColor Cyan
