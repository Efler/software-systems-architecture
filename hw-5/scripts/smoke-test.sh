#!/usr/bin/env bash
set -euo pipefail

base_url="http://localhost:8080/api/v1"
suffix="$(date +%Y%m%d%H%M%S)"
login="cache-check-${suffix}"
email="${login}@example.com"

echo "Create user"
user_response="$(curl -sS -X POST "${base_url}/users" \
  -H "Content-Type: application/json" \
  -d "{\"login\":\"${login}\",\"firstName\":\"Петр\",\"lastName\":\"Смирнов\",\"email\":\"${email}\"}")"
echo "${user_response}"
user_id="$(printf '%s' "${user_response}" | sed -n 's/.*"id":\([0-9][0-9]*\).*/\1/p')"

echo "Find user by login, fills cache"
curl -sS "${base_url}/users?login=${login}"
echo

echo "List services, fills cache"
curl -sS "${base_url}/services"
echo

echo "Create service, invalidates services cache"
service_response="$(curl -sS -X POST "${base_url}/services" \
  -H "Content-Type: application/json" \
  -d '{"name":"Экспресс-консультация","description":"Разовая консультация специалиста","price":1500,"durationMinutes":45}')"
echo "${service_response}"
service_id="$(printf '%s' "${service_response}" | sed -n 's/.*"id":\([0-9][0-9]*\).*/\1/p')"

echo "Add service to order, invalidates order cache"
curl -sS -X POST "${base_url}/orders/services" \
  -H "Content-Type: application/json" \
  -d "{\"userId\":${user_id},\"serviceIds\":[${service_id}]}"
echo

echo "Get order, fills order cache"
curl -sS "${base_url}/orders?userId=${user_id}"
echo
