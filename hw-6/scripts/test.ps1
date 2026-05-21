$ErrorActionPreference = "Stop"

function Invoke-EventRequest {
    param (
        [string]$Uri,
        [string]$Body
    )

    for ($attempt = 1; $attempt -le 10; $attempt++) {
        try {
            return Invoke-RestMethod `
                -Method Post `
                -Uri $Uri `
                -ContentType "application/json" `
                -Body $Body
        }
        catch {
            if ($attempt -eq 10) {
                throw
            }

            Start-Sleep -Seconds 1
        }
    }
}

Write-Host "Publishing UserCreatedEvent..."
Invoke-EventRequest `
    -Uri "http://localhost:8080/users" `
    -Body '{"user_id":"user-1","login":"ivanov","first_name":"Ivan","last_name":"Ivanov","email":"ivanov@example.com"}' |
    ConvertTo-Json -Depth 10

Write-Host "Publishing EventCreatedEvent..."
Invoke-EventRequest `
    -Uri "http://localhost:8080/events" `
    -Body '{"event_id":"event-100","title":"Python Meetup","description":"Meeting for Python developers","event_date":"2026-06-10T18:00:00Z","location":"Moscow","owner_user_id":"user-1"}' |
    ConvertTo-Json -Depth 10

Write-Host "Publishing UserRegisteredForEventEvent..."
Invoke-EventRequest `
    -Uri "http://localhost:8080/events/register" `
    -Body '{"user_id":"user-1","event_id":"event-100"}' |
    ConvertTo-Json -Depth 10

Write-Host "Publishing EventRegistrationCancelledEvent..."
Invoke-EventRequest `
    -Uri "http://localhost:8080/events/cancel" `
    -Body '{"user_id":"user-1","event_id":"event-100"}' |
    ConvertTo-Json -Depth 10

Start-Sleep -Seconds 2

Write-Host "Processed events..."
Invoke-RestMethod `
    -Method Get `
    -Uri "http://localhost:8081/events/processed" | ConvertTo-Json -Depth 10
