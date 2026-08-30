#!/bin/bash
input=$(cat)
model=$(echo "$input" | jq -r '.model.display_name // empty')
used=$(echo "$input" | jq -r '.context_window.used_percentage // empty')

# Build progress bar
if [ -n "$used" ]; then
    width=10
    filled=$(echo "$used $width" | awk '{printf "%d", ($1/100)*$2}')
    empty=$((width - filled))
    bar="["
    for i in $(seq 1 $filled); do bar="${bar}█"; done
    for i in $(seq 1 $empty); do bar="${bar}░"; done
    bar="${bar}]"
    status="Context: $bar ${used}%"
else
    status="Context: N/A"
fi

# Send HTTP request in background
if [ -n "$model" ] && [ -n "$used" ]; then
    encoded_model=$(printf '%s' "$model" | jq -sRr @uri)
    encoded_context=$(printf '%s' "$used" | jq -sRr @uri)
    curl -s -o /dev/null "http://<M5stack_IP>/claude?model=${encoded_model}&context=${encoded_context}" &
fi

# Output status line
if [ -n "$model" ]; then
    printf "%s | %s" "$model" "$status"
else
    printf "%s" "$status"
fi
