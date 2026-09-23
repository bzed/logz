#!/usr/bin/awk -f

function trim(s) {
  sub(/^[[:space:]]+/, "", s)
  sub(/[[:space:]]+$/, "", s)
  return s
}

BEGIN {
  LOG2 = log(2)

  print "| Letter | Flag (Bit) | Decimal | Event Name | Description |"
  print "| :---: | :---: | :---: | :--- | :--- |"
}

# Ищем строки вида: ИМЯ = ЦИФРЫ, // Комментарий
/^[[:space:]]*[A-Z0-9_]+[[:space:]]*=[[:space:]]*[0-9]+,/ {
  name = $1
  val_str = $3
  sub(",", "", val_str)
  val = val_str + 0

  if (val < 1 || name == "MAX")
    next

  bit_idx = int((log(val) / LOG2) + 0.5)

  if (bit_idx < 0)
    next

  # Letters A-Z cover bits 0-25; higher bits are reachable only by the numeric mask.
  letter = "-"
  if (bit_idx <= 25)
    letter = sprintf("%c", 65 + bit_idx)
  desc = ""
  if (index($0, "//") > 0) {
    split($0, parts, "//")
    desc = trim(parts[2])
  }

  printf "| **%s** | `1<<%d` | `%d` | `%s` | %s |\n", letter, bit_idx, val, name, desc
}
