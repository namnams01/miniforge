#!/bin/zsh
echo "===== cal (negative control: expect sys ~0, switches ~0) ====="
/usr/bin/time -l ./sweep cal
echo "===== A (expect sys >> user, switches ~ tasks) ====="
/usr/bin/time -l ./sweep A
echo "===== B (expect user >> sys, modest switches) ====="
/usr/bin/time -l ./sweep B
