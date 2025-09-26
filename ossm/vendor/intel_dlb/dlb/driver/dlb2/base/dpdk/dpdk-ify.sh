#!/bin/bash

IN_FILE=$1
OUT_FILE=$2

cp $IN_FILE $OUT_FILE.tmp

if [[ "$OUT_FILE" =~ .*dlb2_user.h ]]; then
  # Strip the include guard #endif
  sed -i "$(($(wc -l < $OUT_FILE.tmp))),\$d" $OUT_FILE.tmp

  # Append the ioctl code
  cat $OUT_FILE.tmp dlb2_user_ioctl.h.in > $OUT_FILE
else
  cp $OUT_FILE.tmp $OUT_FILE
fi

rm $OUT_FILE.tmp

sed -i 's/INCLUDE_DLB2_USER/dlb2_user.h/g' $OUT_FILE
sed -i 's/INCLUDE_LINUX_TYPES/"dlb2_osdep_types.h"/g' $OUT_FILE
sed -i 's/__iomem//g' $OUT_FILE
