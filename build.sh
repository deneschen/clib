#!/bin/bash
set -e

usage() {
  echo "Usage: $0 --cross --debug --output <out dir> -h"
  exit 1
}

cross=false
debug=false
out=$(pwd)/out

options=$(getopt -o cdho: --long cross,debug,output:,help -n "$0" -- "$@")

eval set -- "$options"

while true; do
  case "$1" in
  -c | --cross)
    cross=true
    shift
    ;;
  -d | --debug)
    debug=true
    shift
    ;;
  -h | --help)
    usage
    shift
    ;;
  -o | --output)
    out="$2"
    shift 2
    ;;
  --)
    shift
    break
    ;;
  *)
    usage
    ;;
  esac
done

cmake -B $out -S .
cmake --build $out