#!/bin/bash

mydir=$(dirname "$0")

set -eu

file1="$1"
file2="$2"

export PATH=$PATH:"$mydir"/../../sonic-annotator

tmproot=/tmp/pitch-track-align-"$$"
trap "rm -f $tmproot.a $tmproot.b" 0

sonic-annotator -t "$mydir/pitch-track.ttl" "$file1" -w csv --csv-one-file "$tmproot.a" --csv-omit-filename --csv-force

sonic-annotator -t "$mydir/pitch-track.ttl" "$file2" -w csv --csv-one-file "$tmproot.b" --csv-omit-filename --csv-force

"$mydir"/pitch-track-align "$tmproot.a" "$tmproot.b"

