#!/bin/bash

# Always run in script-dir
DIR=`dirname $0`
cd $DIR

make check
