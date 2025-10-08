#!/usr/bin/env bash
set -euo pipefail

SERVER_URL="http://127.0.0.1:8080"
REDIS_CLI="redis-cli"
MYSQL="mysql -uroot -proot -h127.0.0.1 -P3306 order_db"

echo "======================================="
echo "[1] Checking if order_server is running"
echo "======================================="
if ! pgrep -x "order_server" >/dev/null; then
  echo "order_server not running. Please start it first (./order_server &)"
  exit 1
fi

sleep 1

echo
echo "======================================="
echo "[2] Checking HTTP connectivity"
echo "======================================="
curl -s -o /dev/null -w "HTTP %http_code\n" "$SERVER_URL/"

echo
echo "======================================="
echo "[3] Health check"
echo "======================================="
if curl -sf "$SERVER_URL/healthz" >/dev/null 2>&1; then
  echo "Health OK "
else
  echo "Health endpoint missing or unhealthy "
fi

echo
echo "======================================="
echo "[4] Creating test order"
echo "======================================="
CREATE_RESP=$(curl -s -X POST "$SERVER_URL/orders" \
  -H "Content-Type: application/json" \
  -d '{"user_id": 1, "item_id": 101, "quantity": 2}')
echo "Response: $CREATE_RESP"

ORDER_ID=$(echo "$CREATE_RESP" | grep -oE '"ORD-[0-9]+' || true)
if [[ -z "$ORDER_ID" ]]; then
  echo "Failed to extract order ID from response!"
else
  echo "Created order ID: $ORDER_ID"
fi

echo
echo "======================================="
echo "[5] Querying order list"
echo "======================================="
curl -s "$SERVER_URL/orders?user_id=1" | jq .

echo
echo "======================================="
echo "[6] Checking MySQL for inserted order"
echo "======================================="
$MYSQL -e "SELECT id,user_id,item_id,quantity,status,created_at FROM orders ORDER BY id DESC LIMIT 5;"

echo
echo "======================================="
echo "[7] Checking Redis cache"
echo "======================================="
$REDIS_CLI keys "order:*" | grep -v "empty" || echo "(no cached keys)"
if [[ -n "$ORDER_ID" ]]; then
  $REDIS_CLI hgetall "order:detail:$ORDER_ID" || true
fi

echo
echo "======================================="
echo "[8] Cleanup and summary"
echo "======================================="
echo " All test steps completed."
