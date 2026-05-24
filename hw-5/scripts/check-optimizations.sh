#!/usr/bin/env bash
set -euo pipefail

base_url="http://localhost:8080/api/v1"
suffix="$(date +%Y%m%d%H%M%S)"

assert() {
  if ! eval "$1"; then
    echo "$2" >&2
    exit 1
  fi
}

redis_get() {
  docker compose exec -T redis redis-cli GET "$1" | tr -d '\r'
}

echo "1. Cache check: list services and verify Redis key"
curl -sS "${base_url}/services" >/dev/null
keys="$(docker compose exec -T redis redis-cli --scan --pattern 'services:list:*')"
if [[ "${keys}" != *"services:list:"* ]]; then
  echo "services list cache key was not created" >&2
  exit 1
fi
echo "   OK: services list cache key exists"

echo "2. Cache invalidation check: create service and verify version increment"
before="$(redis_get 'cache:services:list:version')"
before="${before:-0}"

curl -sS -X POST "${base_url}/services" \
  -H "Content-Type: application/json" \
  -d "{\"name\":\"Контроль кеша ${suffix}\",\"description\":\"Проверка инвалидации каталога\",\"price\":1700,\"durationMinutes\":45}" >/dev/null

after="$(redis_get 'cache:services:list:version')"
if (( after <= before )); then
  echo "services cache version was not incremented" >&2
  exit 1
fi
echo "   OK: version ${before} -> ${after}"

echo "3. Rate limiting check: registration limit must return 429"
client_key="rl-quick-check-${suffix}"
status=0
for i in $(seq 1 11); do
  login="rate-check-${suffix}-${i}"
  status="$(curl -sS -o /dev/null -w "%{http_code}" -X POST "${base_url}/users" \
    -H "X-Forwarded-For: ${client_key}" \
    -H "Content-Type: application/json" \
    -d "{\"login\":\"${login}\",\"firstName\":\"Rate\",\"lastName\":\"Limit\",\"email\":\"${login}@example.com\"}")"
done

if [[ "${status}" != "429" ]]; then
  echo "expected 429 on the 11th registration request, got ${status}" >&2
  exit 1
fi
echo "   OK: 11th registration request returned 429"

echo "4. Rate limit headers check"
login="rate-check-${suffix}-final"
headers="$(mktemp)"
curl -sS -D "${headers}" -o /dev/null -X POST "${base_url}/users" \
  -H "X-Forwarded-For: ${client_key}" \
  -H "Content-Type: application/json" \
  -d "{\"login\":\"${login}\",\"firstName\":\"Rate\",\"lastName\":\"Limit\",\"email\":\"${login}@example.com\"}"

grep -qi '^X-RateLimit-Limit:' "${headers}"
grep -qi '^X-RateLimit-Remaining:' "${headers}"
grep -qi '^X-RateLimit-Reset:' "${headers}"
grep -qi '^Retry-After:' "${headers}"
rm -f "${headers}"
echo "   OK: rate limit headers are present"

echo "All optimization checks passed"
