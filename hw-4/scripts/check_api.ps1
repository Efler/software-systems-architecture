param(
    [string]$ApiUrl = "http://localhost:8080"
)

$ErrorActionPreference = "Stop"

function Assert-True {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Invoke-Json {
    param(
        [string]$Method,
        [string]$Url,
        [object]$Body = $null
    )

    $headers = @{ "Content-Type" = "application/json" }
    if ($null -eq $Body) {
        return Invoke-RestMethod -Method $Method -Uri $Url -Headers $headers
    }

    $jsonBody = $Body | ConvertTo-Json -Depth 10
    return Invoke-RestMethod -Method $Method -Uri $Url -Headers $headers -Body $jsonBody
}

$loginSuffix = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
$login = "check.user.$loginSuffix"

Write-Host "1. Health check"
$health = Invoke-Json -Method "GET" -Url "$ApiUrl/api/v1/health"
Assert-True ($health.status -eq "ok") "API health check failed"

Write-Host "2. Create user through API"
$user = Invoke-Json -Method "POST" -Url "$ApiUrl/api/v1/users" -Body @{
    login = $login
    firstName = "Проверка"
    lastName = "API"
    displayName = "Проверка API"
    email = "$login@example.com"
    position = "QA"
    department = "Platform"
    timezone = "Europe/Moscow"
}
Assert-True ($user.login -eq $login) "Created user login mismatch"
Assert-True (-not [string]::IsNullOrWhiteSpace($user.id)) "Created user id is empty"

Write-Host "3. Find user through API"
$foundUser = Invoke-Json -Method "GET" -Url "$ApiUrl/api/v1/users?login=$login"
Assert-True ($foundUser.id -eq $user.id) "User search by login failed"

Write-Host "4. Create group chat through API"
$chat = Invoke-Json -Method "POST" -Url "$ApiUrl/api/v1/group-chats" -Body @{
    name = "check-chat-$loginSuffix"
    description = "Проверка связки API и MongoDB"
    createdBy = $user.id
    isPrivate = $false
}
Assert-True (-not [string]::IsNullOrWhiteSpace($chat.id)) "Group chat was not created"

Write-Host "5. Add group message through API"
$messageText = "Проверочное сообщение $loginSuffix"
$message = Invoke-Json -Method "POST" -Url "$ApiUrl/api/v1/group-messages" -Body @{
    chatId = $chat.id
    senderId = $user.id
    text = $messageText
}
Assert-True ($message.text -eq $messageText) "Group message was not created"

Write-Host "6. Read group messages through API"
$messages = Invoke-Json -Method "GET" -Url "$ApiUrl/api/v1/group-messages?chatId=$($chat.id)&limit=10"
$messageFound = @($messages.items | Where-Object { $_.id -eq $message.id }).Count -eq 1
Assert-True $messageFound "Created group message was not returned by API"

Write-Host "7. Check MongoDB schema validation"
$validationResult = docker compose -f docker-compose.yml exec -T mongodb mongosh messenger_api --file /scripts/validation.js
$validationText = $validationResult -join "`n"
Assert-True ($validationText -match "Document failed validation") "MongoDB schema validation did not reject invalid documents"
Assert-True ($validationText -match "Final count") "Validation script did not finish correctly"

Write-Host "8. Cleanup temporary data"
$cleanupScript = @"
const userId = ObjectId("$($user.id)");
const chatId = ObjectId("$($chat.id)");
const messageId = ObjectId("$($message.id)");
db.group_messages.deleteOne({ _id: messageId });
db.group_chats.deleteOne({ _id: chatId });
db.users.deleteOne({ _id: userId });
print("cleanup-ok");
"@

$cleanupResult = docker compose -f docker-compose.yml exec -T mongodb mongosh messenger_api --quiet --eval $cleanupScript
Assert-True ($cleanupResult.Trim() -eq "cleanup-ok") "Cleanup failed"

Write-Host "OK: API, MongoDB connection and schema validation are working"
