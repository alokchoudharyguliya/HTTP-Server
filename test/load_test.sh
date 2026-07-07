#!/bin/bash
URL="http://localhost:8001/file.jpeg"
N=5000          # total requests
C=1000          # concurrent

for i in $(seq 1 "$N"); do
  curl -s -o /dev/null -w "req $i -> %{http_code}\n" "$URL" &
  if (( i % C == 0 )); then wait; fi
done
wait