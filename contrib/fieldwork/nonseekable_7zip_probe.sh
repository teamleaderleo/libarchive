#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
	echo "usage: $0 /path/to/bsdtar" >&2
	exit 2
fi

BSDTAR=$1
WORK=${TMPDIR:-/tmp}/libarchive-7zip-probe.$$
RESULTS=$WORK/results.tsv
trap 'rm -rf "$WORK"' EXIT HUP INT TERM
mkdir -p "$WORK/input"

printf 'fieldwork payload\n' >"$WORK/input/payload.txt"
(
	cd "$WORK/input"
	7z a -bd -y "$WORK/sample.7z" payload.txt >/dev/null
)
gzip -c "$WORK/sample.7z" >"$WORK/sample.7z.gz"

printf 'case\tstatus\tstdout\tstderr\n' >"$RESULTS"

run_case() {
	name=$1
	shift
	stdout=$WORK/$name.stdout
	stderr=$WORK/$name.stderr
	status=0
	"$@" >"$stdout" 2>"$stderr" || status=$?
	out=$(tr '\n\t' '  ' <"$stdout" | sed 's/[[:space:]][[:space:]]*/ /g')
	err=$(tr '\n\t' '  ' <"$stderr" | sed 's/[[:space:]][[:space:]]*/ /g')
	printf '%s\t%s\t%s\t%s\n' "$name" "$status" "$out" "$err" >>"$RESULTS"
}

run_case regular-7z-list "$BSDTAR" -tf "$WORK/sample.7z"
run_case direct-7z-pipe-list sh -c 'cat "$1" | "$2" -tf -' sh "$WORK/sample.7z" "$BSDTAR"
run_case wrapped-7z-file-list "$BSDTAR" -tf "$WORK/sample.7z.gz"
run_case wrapped-7z-pipe-list sh -c 'cat "$1" | "$2" -tf -' sh "$WORK/sample.7z.gz" "$BSDTAR"
run_case external-gzip-7z-pipe-list sh -c 'gzip -dc "$1" | "$2" -tf -' sh "$WORK/sample.7z.gz" "$BSDTAR"

run_case regular-7z-extract "$BSDTAR" -xOf "$WORK/sample.7z" payload.txt
run_case direct-7z-pipe-extract sh -c 'cat "$1" | "$2" -xOf - payload.txt' sh "$WORK/sample.7z" "$BSDTAR"
run_case wrapped-7z-file-extract "$BSDTAR" -xOf "$WORK/sample.7z.gz" payload.txt
run_case wrapped-7z-pipe-extract sh -c 'cat "$1" | "$2" -xOf - payload.txt' sh "$WORK/sample.7z.gz" "$BSDTAR"
run_case external-gzip-7z-pipe-extract sh -c 'gzip -dc "$1" | "$2" -xOf - payload.txt' sh "$WORK/sample.7z.gz" "$BSDTAR"

cat "$RESULTS"

list_status=$(awk -F '\t' '$1 == "regular-7z-list" { print $2 }' "$RESULTS")
list_stdout=$(awk -F '\t' '$1 == "regular-7z-list" { print $3 }' "$RESULTS")
if [ "$list_status" -ne 0 ] || ! printf '%s\n' "$list_stdout" | grep -Fq payload.txt; then
	echo "regular seekable 7-Zip listing control failed" >&2
	exit 1
fi

extract_status=$(awk -F '\t' '$1 == "regular-7z-extract" { print $2 }' "$RESULTS")
extract_stdout=$(awk -F '\t' '$1 == "regular-7z-extract" { print $3 }' "$RESULTS")
if [ "$extract_status" -ne 0 ] || [ "$extract_stdout" != "fieldwork payload " ]; then
	echo "regular seekable 7-Zip extraction control failed" >&2
	exit 1
fi

mkdir -p "$GITHUB_WORKSPACE/fieldwork-artifacts"
cp "$RESULTS" "$GITHUB_WORKSPACE/fieldwork-artifacts/nonseekable-7zip-results.tsv"
cp "$WORK"/*.stdout "$WORK"/*.stderr "$GITHUB_WORKSPACE/fieldwork-artifacts/"
