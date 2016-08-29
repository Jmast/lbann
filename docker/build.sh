#!/bin/bash

DIST=$1

function usage() {
    echo "Usage: $0 <distribution>"
}

if [ "$DIST" = "" ]; then
    usage
    echo ""
    echo "Required distribution missing: ubuntu or centos"
    exit 1
fi

echo "building docker image for distribution: $DIST --> lbann-$DIST"

docker build -t lbann-$DIST -f Dockerfile.$DIST . || exit 1

