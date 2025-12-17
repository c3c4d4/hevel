#!/bin/sh
# Convert Plan 9 Cursor definitions in a C file to packed ARGB cursor_data[] + cursor_metadata[].
# Usage:
#   ./p9cursor2rgba.sh 9cursors.c > out.c
#   ./p9cursor2rgba.sh 9cursors.c tl t tr l r bl b br > out.c   # explicit order/subset

infile=${1:-/dev/stdin}
shift 2>/dev/null || true

# Remaining args are optional: desired cursor names in output order.
order="$*"

awk -v ORDER="$order" '
function hexval(c) {
  c = toupper(c)
  if (c >= "0" && c <= "9") return c + 0
  return 10 + index("ABCDEF", c) - 1
}
function hex2dec(s,    i,n) {
  sub(/^0[xX]/, "", s)
  n = 0
  for (i = 1; i <= length(s); i++)
    n = n * 16 + hexval(substr(s, i, 1))
  return n
}
function pow2(n,    i,p) { p=1; for(i=0;i<n;i++) p*=2; return p }
function bit16msb(v, col,    shift) {
  # col: 0..15 from left (MSB)
  shift = 15 - col
  return int(v / P2[shift]) % 2
}
function pix(setb, clrb) {
  # Plan 9 cursor semantics are applied as: dst = (dst & clr) ^ set
  # Per pixel:
  #   clr=1,set=0 => transparent (preserve dst)
  #   clr=0,set=0 => black
  #   clr=0,set=1 => white
  #   clr=1,set=1 => invert (map to white for a usable ARGB cursor)
  if (clrb && !setb) return "0x00000000"
  if (!clrb && setb) return "0xffffffff"
  if (!clrb && !setb) return "0xff000000"
  return "0xffffffff"
}
function min(a,b){ return (a<b)?a:b }
function max(a,b){ return (a>b)?a:b }

BEGIN {
  # precompute powers of two up to 15
  for (i=0;i<16;i++) P2[i]=pow2(i)

  want_cnt = 0
  if (ORDER != "") {
    n = split(ORDER, want, /[[:space:]]+/)
    for (i=1;i<=n;i++) if (want[i] != "") { want_cnt++; wantlist[want[i]]=i }
  }

  in_cursor = 0
  arr = 0
  c1 = c2 = 0
  name_cnt = 0
}

# Start of a cursor block: Cursor foo = {
match($0, /^[[:space:]]*Cursor[[:space:]]+[A-Za-z_][A-Za-z0-9_]*[[:space:]]*=/) {
  # extract the name
  line = $0
  sub(/^[[:space:]]*Cursor[[:space:]]+/, "", line)
  cur = line
  sub(/[[:space:]]*=.*/, "", cur)

  in_cursor = 1
  got_hot = 0
  arr = 0
  c1 = 0
  c2 = 0
  next
}

# Inside cursor: hotspot like {-7, -7},
in_cursor && !got_hot && match($0, /\{[[:space:]]*-?[0-9]+[[:space:]]*,[[:space:]]*-?[0-9]+[[:space:]]*\}/) {
  s = substr($0, RSTART+1, RLENGTH-2)
  gsub(/[[:space:]]*/, "", s)
  split(s, a, ",")
  hx[cur] = a[1] + 0
  hy[cur] = a[2] + 0
  got_hot = 1
}

# Collect bytes: Plan 9 Cursor is offset + clr[32] + set[32]
in_cursor {
  while (match($0, /0[xX][0-9A-Fa-f]+/)) {
    tok = substr($0, RSTART, RLENGTH)
    val = hex2dec(tok)

    if (arr == 0) arr = 1
    if (arr == 1) {
      clr[cur, c1++] = val
      if (c1 >= 32) arr = 2
    } else {
      set[cur, c2++] = val
    }

    $0 = substr($0, RSTART + RLENGTH)
  }
}

# End of cursor block
in_cursor && /};/ {
  in_cursor = 0

  # record encounter order if not already
  if (!(seen[cur]++)) {
    name_cnt++
    names[name_cnt] = cur
  }

  if (c1 < 32 || c2 < 32) {
    # allow partial, but warn
    warn[cur] = 1
  }
}

END {
  # decide output order
  outn = 0
  if (want_cnt > 0) {
    for (i=1; i<=want_cnt; i++) out[++outn] = want[i]
  } else {
    for (i=1; i<=name_cnt; i++) out[++outn] = names[i]
  }

  print "/* generated from Plan 9 Cursor data */"
  print "#include <stdint.h>"
  print "#include <stddef.h>"
  print ""
  print "enum {"
  for (oi=1; oi<=outn; oi++) {
    n = out[oi]
    if (n == "") continue
    name = toupper(n)
    gsub(/[^A-Z0-9]+/, "_", name)
    printf "\tNEIN_CURSOR_%s = %d,\n", name, oi-1
  }
  print "};"
  print ""
  print "static const uint32_t nein_cursor_data[] = {"

  off = 0
  perline = 0

  for (oi=1; oi<=outn; oi++) {
    n = out[oi]
    if (n == "") continue
    if (!(seen[n])) {
      printf "/* WARNING: cursor %s not found */\n", n
      continue
    }
    if (warn[n]) {
      printf "/* WARNING: cursor %s looked incomplete (expected 32 clr + 32 set bytes) */\n", n
    }

    # build full 16x16 pixels and compute tight bbox
    minx=16; miny=16; maxx=-1; maxy=-1
    for (y=0; y<16; y++) {
      rowc = clr[n,2*y]*256 + clr[n,2*y+1]
      rows = set[n,2*y]*256 + set[n,2*y+1]
      for (x=0; x<16; x++) {
        pb = pix(bit16msb(rows,x), bit16msb(rowc,x))
        full[n,y,x] = pb
        if (pb != "0x00000000") {
          minx = min(minx, x); maxx = max(maxx, x)
          miny = min(miny, y); maxy = max(maxy, y)
        }
      }
    }

    # if entirely transparent, fall back to 1x1
    if (maxx < 0) { minx=0; maxx=0; miny=0; maxy=0 }

    w = maxx - minx + 1
    h = maxy - miny + 1

    # Plan 9 Cursor.offset is the top-left draw offset relative to the mouse.
    # Therefore hotspot (the mouse point within the 16x16 bitmap) is -offset.
    xhot = -hx[n]
    yhot = -hy[n]

    hx2 = xhot - minx
    hy2 = yhot - miny

    meta_name[oi] = n
    meta_w[oi] = w
    meta_h[oi] = h
    meta_hx[oi] = hx2
    meta_hy[oi] = hy2
    meta_off[oi] = off

    # emit cropped pixels row-major
    for (y=miny; y<=maxy; y++) {
      for (x=minx; x<=maxx; x++) {
        v = full[n,y,x]
        if (perline == 0) printf "\t"
        printf "%s,", v
        perline++
        off++
        if (perline >= 6) { printf "\n"; perline = 0 }
        else printf " "
      }
    }
    if (perline != 0) { printf "\n"; perline = 0 }
  }

  print "};"
  print ""
  print "struct nein_cursor_meta {"
  print "\tuint32_t width, height;"
  print "\tint32_t hotspot_x, hotspot_y;"
  print "\tsize_t offset;"
  print "};"
  print ""
  print "static const struct nein_cursor_meta nein_cursor_metadata[] = {"

  for (oi=1; oi<=outn; oi++) {
    n = meta_name[oi]
    if (n == "") continue
    printf "\t{ %d, %d, %d, %d, %d }, /* %s */\n",
      meta_w[oi], meta_h[oi], meta_hx[oi], meta_hy[oi], meta_off[oi], n
  }

  print "};"
}
' "$infile"
