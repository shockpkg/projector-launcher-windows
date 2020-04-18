#!/bin/bash
set -o errexit
set -o nounset
set -o pipefail

build='build'
archs=(
	'i686'
	'x86_64'
)

rm -rf "${build}"
mkdir -p "${build}"

for arch in "${archs[@]}"; do
	out="${build}/main.${arch}.exe"
	"${arch}-w64-mingw32-gcc" \
		-Wall \
		-mwindows \
		-static \
		-O3 -s \
		-o "${out}" \
		'src/main.c'
	"${arch}-w64-mingw32-strip" -sg "${out}"
done

cd "${build}"
for arch in "${archs[@]}"; do
	shasum -a 256 "main.${arch}.exe" > "main.${arch}.exe.sha256"
done
