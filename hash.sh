#!/bin/bash
set -o errexit
set -o nounset
set -o pipefail

archs=(
	'i686'
	'x86_64'
)

cd 'build'
for arch in "${archs[@]}"; do
	shasum -a 256 "main.${arch}.exe" > "main.${arch}.exe.sha256"
done
