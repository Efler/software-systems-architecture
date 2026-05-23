$ErrorActionPreference = "Stop"

$baseUrl = "http://localhost:8080/api/v1"
$suffix = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
$login = "warehouse-user-$suffix"
$sku = "SKU-$suffix"

function Invoke-Api {
    param(
        [string]$Method,
        [string]$Path,
        [string]$Body = "",
        [hashtable]$Headers = @{}
    )

    $params = @{
        Method = $Method
        Uri = "$baseUrl$Path"
        Headers = $Headers
        SkipHttpErrorCheck = $true
    }

    if ($Body -ne "") {
        $params.ContentType = "application/json"
        $params.Body = $Body
    }

    $response = Invoke-WebRequest @params
    $json = $null
    if ($response.Content) {
        $json = $response.Content | ConvertFrom-Json
    }

    [PSCustomObject]@{
        Status = [int]$response.StatusCode
        Body = $json
        Raw = $response.Content
    }
}

function Assert-Status {
    param(
        [object]$Response,
        [int]$Expected,
        [string]$Name
    )

    if ($Response.Status -ne $Expected) {
        throw "${Name}: expected HTTP $Expected, got HTTP $($Response.Status). Body: $($Response.Raw)"
    }
    Write-Host "[OK] $Name -> HTTP $Expected"
}

function Assert-Equals {
    param(
        [object]$Actual,
        [object]$Expected,
        [string]$Name
    )

    if ($Actual -ne $Expected) {
        throw "${Name}: expected '$Expected', got '$Actual'"
    }
}

function Assert-True {
    param(
        [bool]$Condition,
        [string]$Name
    )

    if (-not $Condition) {
        throw "${Name}: condition failed"
    }
}

Write-Host "Running Warehouse Inventory API tests"

$register = Invoke-Api "POST" "/auth/register" "{`"login`":`"$login`",`"firstName`":`"Test`",`"lastName`":`"User`",`"email`":`"$login@example.com`",`"role`":`"warehouse_manager`",`"password`":`"secret`"}"
Assert-Status $register 201 "register user"
Assert-True ($null -ne $register.Body.token) "register returns token"
$userId = $register.Body.user.id

$duplicateRegister = Invoke-Api "POST" "/auth/register" "{`"login`":`"$login`",`"firstName`":`"Test`",`"lastName`":`"User`",`"email`":`"$login@example.com`",`"role`":`"warehouse_manager`",`"password`":`"secret`"}"
Assert-Status $duplicateRegister 409 "duplicate registration"

$invalidRegister = Invoke-Api "POST" "/auth/register" "{`"login`":`"bad-$suffix`"}"
Assert-Status $invalidRegister 422 "invalid registration payload"

$loginResponse = Invoke-Api "POST" "/auth/login" "{`"login`":`"$login`",`"password`":`"secret`"}"
Assert-Status $loginResponse 200 "login user"
$headers = @{ Authorization = "Bearer $($loginResponse.Body.token)" }

$badLogin = Invoke-Api "POST" "/auth/login" "{`"login`":`"$login`",`"password`":`"wrong`"}"
Assert-Status $badLogin 401 "login with wrong password"

$createProductNoToken = Invoke-Api "POST" "/products" "{`"name`":`"Protected product`",`"sku`":`"$sku-NOAUTH`",`"unit`":`"pcs`"}"
Assert-Status $createProductNoToken 401 "create product without token"

$invalidProduct = Invoke-Api "POST" "/products" "{`"name`":`"No sku`"}" $headers
Assert-Status $invalidProduct 422 "create product with invalid payload"

$product = Invoke-Api "POST" "/products" "{`"name`":`"Inventory monitor $suffix`",`"sku`":`"$sku`",`"unit`":`"pcs`",`"description`":`"Inventory monitor`"}" $headers
Assert-Status $product 201 "create product"
Assert-Equals $product.Body.sku $sku "created product sku"
$productId = $product.Body.id

$duplicateProduct = Invoke-Api "POST" "/products" "{`"name`":`"Inventory monitor duplicate`",`"sku`":`"$sku`",`"unit`":`"pcs`"}" $headers
Assert-Status $duplicateProduct 409 "duplicate product sku"

$receipt = Invoke-Api "POST" "/receipts" "{`"productId`":$productId,`"quantity`":10,`"createdBy`":$userId,`"comment`":`"Initial delivery`"}" $headers
Assert-Status $receipt 201 "create receipt"
Assert-Equals $receipt.Body.stockBalance.quantity 10 "receipt updates stock balance"

$missingProductReceipt = Invoke-Api "POST" "/receipts" "{`"productId`":999999,`"quantity`":1,`"createdBy`":$userId,`"comment`":`"Missing product`"}" $headers
Assert-Status $missingProductReceipt 404 "receipt for missing product"

$missingUserReceipt = Invoke-Api "POST" "/receipts" "{`"productId`":$productId,`"quantity`":1,`"createdBy`":999999,`"comment`":`"Missing user`"}" $headers
Assert-Status $missingUserReceipt 404 "receipt by missing user"

$writeOff = Invoke-Api "POST" "/write-offs" "{`"productId`":$productId,`"quantity`":3,`"createdBy`":$userId,`"reason`":`"Inventory adjustment`"}" $headers
Assert-Status $writeOff 201 "create write-off"
Assert-Equals $writeOff.Body.stockBalance.quantity 7 "write-off updates stock balance"

$tooLargeWriteOff = Invoke-Api "POST" "/write-offs" "{`"productId`":$productId,`"quantity`":1000,`"createdBy`":$userId,`"reason`":`"Too much`"}" $headers
Assert-Status $tooLargeWriteOff 409 "write-off with insufficient stock"

$userByLogin = Invoke-Api "GET" "/users?login=$login"
Assert-Status $userByLogin 200 "find user by login"
Assert-Equals $userByLogin.Body.login $login "found user login"

$usersByMask = Invoke-Api "GET" "/users?firstNameMask=Te&lastNameMask=Us"
Assert-Status $usersByMask 200 "find users by name mask"
Assert-True ($usersByMask.Body.total -ge 1) "users by mask returns items"

$missingUser = Invoke-Api "GET" "/users?login=missing-$suffix"
Assert-Status $missingUser 404 "find missing user"

$products = Invoke-Api "GET" "/products?name=monitor"
Assert-Status $products 200 "find products by name"
Assert-True ($products.Body.total -ge 1) "products search returns items"

$productsWithoutName = Invoke-Api "GET" "/products"
Assert-Status $productsWithoutName 400 "find products without name"

$balances = Invoke-Api "GET" "/stock-balances?productId=$productId"
Assert-Status $balances 200 "get stock balance by product"
Assert-Equals $balances.Body.items[0].quantity 7 "stock balance quantity"

$invalidBalanceRequest = Invoke-Api "GET" "/stock-balances?productId=abc"
Assert-Status $invalidBalanceRequest 400 "stock balance with invalid productId"

$missingBalanceProduct = Invoke-Api "GET" "/stock-balances?productId=999999"
Assert-Status $missingBalanceProduct 404 "stock balance for missing product"

$receipts = Invoke-Api "GET" "/receipts?productId=$productId"
Assert-Status $receipts 200 "list receipts by product"
Assert-Equals $receipts.Body.total 1 "receipt history total"

$invalidReceiptsRequest = Invoke-Api "GET" "/receipts?productId=abc"
Assert-Status $invalidReceiptsRequest 400 "receipts with invalid productId"

Write-Host "All API tests passed"
