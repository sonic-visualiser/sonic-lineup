#!/bin/bash

mydir=$(dirname "$0")

set -eu

file1="$1"
file2="$2"

export PATH=$PATH:"$mydir"/../../sonic-annotator

tmproot=/tmp/pitch-track-align-"$$"
trap "rm -f $tmproot.a $tmproot.b" 0

#sonic-annotator -t "$mydir/pitch-track.ttl" "$file1" -w csv --csv-one-file "$tmproot.a" --csv-omit-filename --csv-force
#sonic-annotator -t "$mydir/pitch-track.ttl" "$file2" -w csv --csv-one-file "$tmproot.b" --csv-omit-filename --csv-force

sonic-annotator -t "$mydir/note-track.ttl" "$file1" -w csv --csv-omit-filename --csv-stdout | awk -F, '{ print $1 "," $3 }' > "$tmproot.a"
sonic-annotator -t "$mydir/note-track.ttl" "$file2" -w csv --csv-omit-filename --csv-stdout | awk -F, '{ print $1 "," $3 }' > "$tmproot.b"

echo 1>&2
echo "First track:" 1>&2
cat "$tmproot.a" 1>&2

echo 1>&2
echo "Second track:" 1>&2
cat "$tmproot.b" 1>&2

echo "0,0"

"$mydir"/pitch-track-align "$tmproot.a" "$tmproot.b"

