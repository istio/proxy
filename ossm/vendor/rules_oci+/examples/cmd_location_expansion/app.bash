#/bin/bash
echo "hello world!"

args=( "$@" )
ELEMENTS=${#args[@]}
for (( i=0;i<$ELEMENTS;i++)); do
    val="${args[${i}]}"
    echo "arg ${i}:${val}"
done