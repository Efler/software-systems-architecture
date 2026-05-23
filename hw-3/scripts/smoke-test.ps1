$ErrorActionPreference = "Stop"

$baseUrl = "http://localhost:8080/api/v1"
$suffix = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
$login = "ivanov-$suffix"
$sku = "MON-24-$suffix"

function Invoke-JsonPost {
    param(
        [string]$Uri,
        [string]$Body,
        [hashtable]$Headers = @{}
    )

    Invoke-RestMethod -Method Post -Uri $Uri -ContentType "application/json" -Body $Body -Headers $Headers
}

Write-Host "Register user"
$registerResponse = Invoke-JsonPost "$baseUrl/auth/register" "{`"login`":`"$login`",`"firstName`":`"Ivan`",`"lastName`":`"Ivanov`",`"email`":`"$login@example.com`",`"role`":`"warehouse_manager`",`"password`":`"secret`"}"
$registerResponse | ConvertTo-Json -Depth 10
$userId = $registerResponse.user.id

Write-Host "Login user"
$loginResponse = Invoke-JsonPost "$baseUrl/auth/login" "{`"login`":`"$login`",`"password`":`"secret`"}"
$loginResponse | ConvertTo-Json -Depth 10

$headers = @{
    Authorization = "Bearer $($loginResponse.token)"
}

Write-Host "Create product"
$productResponse = Invoke-JsonPost "$baseUrl/products" "{`"name`":`"Office monitor 24 $suffix`",`"sku`":`"$sku`",`"unit`":`"pcs`",`"description`":`"Office monitor`"}" $headers
$productResponse | ConvertTo-Json -Depth 10
$productId = $productResponse.id

Write-Host "Create receipt"
Invoke-JsonPost "$baseUrl/receipts" "{`"productId`":$productId,`"quantity`":10,`"createdBy`":$userId,`"comment`":`"Initial delivery`"}" $headers | ConvertTo-Json -Depth 10

Write-Host "Write off product"
Invoke-JsonPost "$baseUrl/write-offs" "{`"productId`":$productId,`"quantity`":2,`"createdBy`":$userId,`"reason`":`"Damaged package`"}" $headers | ConvertTo-Json -Depth 10

Write-Host "Search user by login"
Invoke-RestMethod -Method Get -Uri "$baseUrl/users?login=$login" | ConvertTo-Json -Depth 10

Write-Host "Search products by name"
Invoke-RestMethod -Method Get -Uri "$baseUrl/products?name=$suffix" | ConvertTo-Json -Depth 10

Write-Host "Stock balances"
Invoke-RestMethod -Method Get -Uri "$baseUrl/stock-balances?productId=$productId" | ConvertTo-Json -Depth 10

Write-Host "Receipt history"
Invoke-RestMethod -Method Get -Uri "$baseUrl/receipts?productId=$productId" | ConvertTo-Json -Depth 10

Write-Host "Unauthorized write-off check"
$unauthorized = Invoke-WebRequest -Method Post -Uri "$baseUrl/write-offs" -ContentType "application/json" -Body "{`"productId`":$productId,`"quantity`":1,`"createdBy`":$userId,`"reason`":`"No token`"}" -SkipHttpErrorCheck
"Status: $($unauthorized.StatusCode)"
$unauthorized.Content
