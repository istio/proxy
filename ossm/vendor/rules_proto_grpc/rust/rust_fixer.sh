#!/bin/bash

set -eo pipefail

cp $1/* $2
cd $2
chmod +w *

for base in $(ls | grep -v "^mod.rs$\|.serde.rs$\|.tonic.rs$"); do 
    for f in $(ls $(echo "$base" | sed 's/.rs$//').{serde,tonic}.rs 2>/dev/null); do
        echo "include!(\"$f\");" >> $base;
    done
done
