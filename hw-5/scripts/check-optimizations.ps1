$ErrorActionPreference = "Stop"

$baseUrl = "http://localhost:8080/api/v1"
$suffix = Get-Date -Format "yyyyMMddHHmmss"

function RedisValue($key) {
  $value = docker compose exec -T redis redis-cli GET $key
  if ($LASTEXITCODE -ne 0) {
    throw "redis-cli GET $key failed"
  }
  return ($value | Out-String).Trim()
}

function Assert($condition, $message) {
  if (-not $condition) {
    throw $message
  }
}

Write-Host "1. Cache check: list services and verify Redis key"
Invoke-RestMethod "$baseUrl/services" | Out-Null
$keys = docker compose exec -T redis redis-cli --scan --pattern "services:list:*"
Assert (($keys | Out-String).Contains("services:list:")) "services list cache key was not created"
Write-Host "   OK: services list cache key exists"

Write-Host "2. Cache invalidation check: create service and verify version increment"
$beforeRaw = RedisValue "cache:services:list:version"
$before = if ($beforeRaw) { [int]$beforeRaw } else { 0 }

Invoke-RestMethod -Method Post "$baseUrl/services" `
  -ContentType "application/json" `
  -Body (@{
    name = "Контроль кеша $suffix"
    description = "Проверка инвалидации каталога"
    price = 1700
    durationMinutes = 45
  } | ConvertTo-Json) | Out-Null

$after = [int](RedisValue "cache:services:list:version")
Assert ($after -gt $before) "services cache version was not incremented"
Write-Host "   OK: version $before -> $after"

Write-Host "3. Rate limiting check: registration limit must return 429"
$clientKey = "rl-quick-check-$suffix"
$status = 0
for ($i = 1; $i -le 11; $i++) {
  $login = "rate-check-$suffix-$i"
  $body = @{
    login = $login
    firstName = "Rate"
    lastName = "Limit"
    email = "$login@example.com"
  } | ConvertTo-Json

  $response = Invoke-WebRequest -SkipHttpErrorCheck -Method Post "$baseUrl/users" `
    -Headers @{ "X-Forwarded-For" = $clientKey } `
    -ContentType "application/json" `
    -Body $body
  $status = $response.StatusCode
}

Assert ($status -eq 429) "expected 429 on the 11th registration request, got $status"
Write-Host "   OK: 11th registration request returned 429"

Write-Host "4. Rate limit headers check"
$response = Invoke-WebRequest -SkipHttpErrorCheck -Method Post "$baseUrl/users" `
  -Headers @{ "X-Forwarded-For" = $clientKey } `
  -ContentType "application/json" `
  -Body (@{
    login = "rate-check-$suffix-final"
    firstName = "Rate"
    lastName = "Limit"
    email = "rate-check-$suffix-final@example.com"
  } | ConvertTo-Json)

Assert ($response.Headers["X-RateLimit-Limit"]) "missing X-RateLimit-Limit"
Assert ($response.Headers["X-RateLimit-Remaining"] -ne $null) "missing X-RateLimit-Remaining"
Assert ($response.Headers["X-RateLimit-Reset"]) "missing X-RateLimit-Reset"
Assert ($response.Headers["Retry-After"]) "missing Retry-After"
Write-Host "   OK: rate limit headers are present"

Write-Host "All optimization checks passed"
