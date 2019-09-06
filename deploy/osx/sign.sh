#!/bin/bash 

set -eu

# Execute this from the top-level directory of the project (the one
# that contains the .app bundle).  Supply the name of the .app bundle
# as argument
dir="$1"
if [ -z "$dir" ] || [ ! -d "$dir" ]; then
	echo "Usage: $0 <pkgdir>"
	echo "Where pkgdir is the directory containing <MyApplication>.app"
	echo "All .app bundles in pkgdir will be signed"
	exit 2
fi

if false; then

    echo
    echo "Experimentally attempting sandboxing + hardened runtime (this won't work)"
    echo

    entitlements=deploy/osx/Entitlements.plist

    for app in "$dir"/*.app; do
	find "$app" -name \*.dylib -print | while read fr; do
	    codesign -s "Developer ID Application: Chris Cannam" -fv --deep --options runtime "$fr"
	done
	codesign -s "Developer ID Application: Chris Cannam" -fv --deep --options runtime --entitlements "$entitlements" "$app/Contents/MacOS/Sonic Lineup"
	codesign -s "Developer ID Application: Chris Cannam" -fv --deep --options runtime --entitlements "$entitlements" "$app"
    done

else

    echo
    echo "Not applying sandboxing or hardened runtime"
    echo

    for app in "$dir"/*.app; do
	find "$app" -name \*.dylib -print | while read fr; do
	    codesign -s "Developer ID Application: Chris Cannam" -fv --deep "$fr"
	done
	codesign -s "Developer ID Application: Chris Cannam" -fv --deep "$app/Contents/MacOS/Sonic Lineup"
	codesign -s "Developer ID Application: Chris Cannam" -fv --deep "$app"
    done

fi


