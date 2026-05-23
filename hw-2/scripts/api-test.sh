#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://localhost:8080/api/v1}"
SUFFIX="$(date +%s%3N 2>/dev/null || python3 - <<'PY'
import time
print(int(time.time() * 1000))
PY
)"
LOGIN="warehouse-user-${SUFFIX}"
SKU="SKU-${SUFFIX}"

RESPONSE_STATUS=""
RESPONSE_BODY=""

request() {
  local method="$1"
  local path="$2"
  local body="${3:-}"
  local token="${4:-}"
  local tmp
  tmp="$(mktemp)"

  local args=(-sS -w "%{http_code}" -o "$tmp" -X "$method" "${BASE_URL}${path}")

  if [[ -n "$body" ]]; then
    args+=(-H "Content-Type: application/json" -d "$body")
  fi

  if [[ -n "$token" ]]; then
    args+=(-H "Authorization: Bearer ${token}")
  fi

  RESPONSE_STATUS="$(curl "${args[@]}")"
  RESPONSE_BODY="$(cat "$tmp")"
  rm -f "$tmp"
}

json_get() {
  local expr="$1"
  RESPONSE_BODY="$RESPONSE_BODY" python3 - "$expr" <<'PY'
import json
import os
import sys

data = json.loads(os.environ["RESPONSE_BODY"])
value = data
for part in sys.argv[1].split("."):
    if part.isdigit():
        value = value[int(part)]
    else:
        value = value[part]
print(value)
PY
}

assert_status() {
  local expected="$1"
  local name="$2"
  if [[ "$RESPONSE_STATUS" != "$expected" ]]; then
    echo "${name}: expected HTTP ${expected}, got HTTP ${RESPONSE_STATUS}. Body: ${RESPONSE_BODY}" >&2
    exit 1
  fi
  echo "[OK] ${name} -> HTTP ${expected}"
}

assert_equals() {
  local actual="$1"
  local expected="$2"
  local name="$3"
  if [[ "$actual" != "$expected" ]]; then
    echo "${name}: expected '${expected}', got '${actual}'" >&2
    exit 1
  fi
}

assert_number_ge() {
  local actual="$1"
  local expected="$2"
  local name="$3"
  if (( actual < expected )); then
    echo "${name}: expected >= ${expected}, got ${actual}" >&2
    exit 1
  fi
}

echo "Running Warehouse Inventory API tests"

request POST "/auth/register" "{\"login\":\"${LOGIN}\",\"firstName\":\"Warehouse\",\"lastName\":\"Operator\",\"email\":\"${LOGIN}@example.com\",\"role\":\"warehouse_manager\",\"password\":\"secret\"}"
assert_status 201 "register user"
USER_ID="$(json_get "user.id")"

request POST "/auth/register" "{\"login\":\"${LOGIN}\",\"firstName\":\"Warehouse\",\"lastName\":\"Operator\",\"email\":\"${LOGIN}@example.com\",\"role\":\"warehouse_manager\",\"password\":\"secret\"}"
assert_status 409 "duplicate registration"

request POST "/auth/register" "{\"login\":\"bad-${SUFFIX}\"}"
assert_status 422 "invalid registration payload"

request POST "/auth/login" "{\"login\":\"${LOGIN}\",\"password\":\"secret\"}"
assert_status 200 "login user"
TOKEN="$(json_get "token")"

request POST "/auth/login" "{\"login\":\"${LOGIN}\",\"password\":\"wrong\"}"
assert_status 401 "login with wrong password"

request POST "/products" "{\"name\":\"Protected product\",\"sku\":\"${SKU}-NOAUTH\",\"unit\":\"pcs\"}"
assert_status 401 "create product without token"

request POST "/products" "{\"name\":\"No sku\"}" "$TOKEN"
assert_status 422 "create product with invalid payload"

request POST "/products" "{\"name\":\"Inventory monitor ${SUFFIX}\",\"sku\":\"${SKU}\",\"unit\":\"pcs\",\"description\":\"Inventory monitor\"}" "$TOKEN"
assert_status 201 "create product"
assert_equals "$(json_get "sku")" "$SKU" "created product sku"
PRODUCT_ID="$(json_get "id")"

request POST "/products" "{\"name\":\"Inventory monitor duplicate\",\"sku\":\"${SKU}\",\"unit\":\"pcs\"}" "$TOKEN"
assert_status 409 "duplicate product sku"

request POST "/receipts" "{\"productId\":${PRODUCT_ID},\"quantity\":10,\"createdBy\":${USER_ID},\"comment\":\"Initial delivery\"}" "$TOKEN"
assert_status 201 "create receipt"
assert_equals "$(json_get "stockBalance.quantity")" "10" "receipt updates stock balance"

request POST "/receipts" "{\"productId\":999999,\"quantity\":1,\"createdBy\":${USER_ID},\"comment\":\"Missing product\"}" "$TOKEN"
assert_status 404 "receipt for missing product"

request POST "/receipts" "{\"productId\":${PRODUCT_ID},\"quantity\":1,\"createdBy\":999999,\"comment\":\"Missing user\"}" "$TOKEN"
assert_status 404 "receipt by missing user"

request POST "/write-offs" "{\"productId\":${PRODUCT_ID},\"quantity\":3,\"createdBy\":${USER_ID},\"reason\":\"Inventory adjustment\"}" "$TOKEN"
assert_status 201 "create write-off"
assert_equals "$(json_get "stockBalance.quantity")" "7" "write-off updates stock balance"

request POST "/write-offs" "{\"productId\":${PRODUCT_ID},\"quantity\":1000,\"createdBy\":${USER_ID},\"reason\":\"Too much\"}" "$TOKEN"
assert_status 409 "write-off with insufficient stock"

request GET "/users?login=${LOGIN}"
assert_status 200 "find user by login"
assert_equals "$(json_get "login")" "$LOGIN" "found user login"

request GET "/users?firstNameMask=Ware&lastNameMask=Oper"
assert_status 200 "find users by name mask"
assert_number_ge "$(json_get "total")" 1 "users by mask returns items"

request GET "/users?login=missing-${SUFFIX}"
assert_status 404 "find missing user"

request GET "/products?name=monitor"
assert_status 200 "find products by name"
assert_number_ge "$(json_get "total")" 1 "products search returns items"

request GET "/products"
assert_status 400 "find products without name"

request GET "/stock-balances?productId=${PRODUCT_ID}"
assert_status 200 "get stock balance by product"
assert_equals "$(json_get "items.0.quantity")" "7" "stock balance quantity"

request GET "/stock-balances?productId=abc"
assert_status 400 "stock balance with invalid productId"

request GET "/stock-balances?productId=999999"
assert_status 404 "stock balance for missing product"

request GET "/receipts?productId=${PRODUCT_ID}"
assert_status 200 "list receipts by product"
assert_equals "$(json_get "total")" "1" "receipt history total"

request GET "/receipts?productId=abc"
assert_status 400 "receipts with invalid productId"

echo "All API tests passed"
