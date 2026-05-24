$ErrorActionPreference = "Stop"
$baseUrl = "http://localhost:8080/api/v1"
$suffix = Get-Date -Format "yyyyMMddHHmmss"
$login = "cache-check-$suffix"
$email = "$login@example.com"

Write-Host "Create user"
$user = Invoke-RestMethod -Method Post "$baseUrl/users" `
  -ContentType "application/json" `
  -Body (@{ login = $login; firstName = "Петр"; lastName = "Смирнов"; email = $email } | ConvertTo-Json)
$user

Write-Host "Find user by login, fills cache"
Invoke-RestMethod "$baseUrl/users?login=$login"

Write-Host "List services, fills cache"
Invoke-RestMethod "$baseUrl/services"

Write-Host "Create service, invalidates services cache"
$service = Invoke-RestMethod -Method Post "$baseUrl/services" `
  -ContentType "application/json" `
  -Body '{"name":"Экспресс-консультация","description":"Разовая консультация специалиста","price":1500,"durationMinutes":45}'
$service

Write-Host "Add service to order, invalidates order cache"
Invoke-RestMethod -Method Post "$baseUrl/orders/services" `
  -ContentType "application/json" `
  -Body (@{ userId = $user.id; serviceIds = @($service.id) } | ConvertTo-Json)

Write-Host "Get order, fills order cache"
Invoke-RestMethod "$baseUrl/orders?userId=$($user.id)"
