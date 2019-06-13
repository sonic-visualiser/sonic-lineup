#!/bin/bash

tag=`hg tags | grep '^vect_v' | head -1 | awk '{ print $1; }'`

v=`echo "$tag" |sed 's/vect_v//'`

echo "Packaging up version $v from tag $tag..."

hg archive -r"$tag" --subrepos --exclude sv-dependency-builds /tmp/sonic-lineup-"$v".tar.gz

