#!/bin/bash

dir=$1

[ -d "$dir" ] || exit 1

set -eu

strip "$dir"/usr/bin/*
strip "$dir"/usr/lib/*/*.so
strip "$dir"/usr/lib/*/vamp-plugin-load-checker

sz=`du -sx --exclude DEBIAN "$dir" | awk '{ print $1; }'`
perl -i -p -e "s/Installed-Size: .*/Installed-Size: $sz/" "$dir"/DEBIAN/control

find "$dir" -name \*~ -exec rm \{\} \;

chown -R root.root "$dir"/*

chmod -R g-w "$dir"/*
