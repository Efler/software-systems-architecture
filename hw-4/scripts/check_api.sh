#!/usr/bin/env bash
set -euo pipefail

API_URL="${API_URL:-http://localhost:8080}"
LOGIN_SUFFIX="$(date +%s)"
LOGIN="check.user.${LOGIN_SUFFIX}"

request() {
  local method="$1"
  local url="$2"
  local body="${3:-}"

  if [[ -z "$body" ]]; then
    curl -fsS -X "$method" "$url"
  else
    curl -fsS -X "$method" "$url" \
      -H "Content-Type: application/json" \
      -d "$body"
  fi
}

json_get() {
  python3 -c "import json,sys; print(json.load(sys.stdin)$1)"
}

echo "1. Health check"
health="$(request GET "$API_URL/api/v1/health")"
[[ "$(printf '%s' "$health" | json_get "['status']")" == "ok" ]]

echo "2. Create user through API"
user="$(request POST "$API_URL/api/v1/users" "{
  \"login\":\"$LOGIN\",
  \"firstName\":\"Проверка\",
  \"lastName\":\"API\",
  \"displayName\":\"Проверка API\",
  \"email\":\"$LOGIN@example.com\",
  \"position\":\"QA\",
  \"department\":\"Platform\",
  \"timezone\":\"Europe/Moscow\"
}")"
user_id="$(printf '%s' "$user" | json_get "['id']")"
[[ -n "$user_id" ]]

echo "3. Find user through API"
found_user="$(request GET "$API_URL/api/v1/users?login=$LOGIN")"
[[ "$(printf '%s' "$found_user" | json_get "['id']")" == "$user_id" ]]

echo "4. Create group chat through API"
chat="$(request POST "$API_URL/api/v1/group-chats" "{
  \"name\":\"check-chat-$LOGIN_SUFFIX\",
  \"description\":\"Проверка связки API и MongoDB\",
  \"createdBy\":\"$user_id\",
  \"isPrivate\":false
}")"
chat_id="$(printf '%s' "$chat" | json_get "['id']")"
[[ -n "$chat_id" ]]

echo "5. Add group message through API"
message_text="Проверочное сообщение $LOGIN_SUFFIX"
message="$(request POST "$API_URL/api/v1/group-messages" "{
  \"chatId\":\"$chat_id\",
  \"senderId\":\"$user_id\",
  \"text\":\"$message_text\"
}")"
message_id="$(printf '%s' "$message" | json_get "['id']")"
[[ -n "$message_id" ]]

echo "6. Read group messages through API"
messages="$(request GET "$API_URL/api/v1/group-messages?chatId=$chat_id&limit=10")"
printf '%s' "$messages" | python3 -c "import json,sys; data=json.load(sys.stdin); assert any(item['id'] == '$message_id' for item in data['items'])"

echo "7. Check MongoDB schema validation"
validation_result="$(docker compose -f docker-compose.yml exec -T mongodb mongosh messenger_api --file /scripts/validation.js)"
printf '%s' "$validation_result" | grep -q "Document failed validation"
printf '%s' "$validation_result" | grep -q "Final count"

echo "8. Cleanup temporary data"
cleanup_result="$(docker compose -f docker-compose.yml exec -T mongodb mongosh messenger_api --quiet --eval "
db.group_messages.deleteOne({ _id: ObjectId('$message_id') });
db.group_chats.deleteOne({ _id: ObjectId('$chat_id') });
db.users.deleteOne({ _id: ObjectId('$user_id') });
print('cleanup-ok');
")"
[[ "$cleanup_result" == "cleanup-ok" ]]

echo "OK: API, MongoDB connection and schema validation are working"
