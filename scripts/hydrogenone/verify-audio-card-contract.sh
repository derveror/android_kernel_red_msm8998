#!/bin/bash

set -euo pipefail

fail() {
	printf 'FAIL: %s\n' "$*" >&2
	exit 1
}

test "$#" -eq 1 || fail "usage: $0 <kernel-output-dir>"
out=${1%/}
dtc="$out/scripts/dtc/dtc"
test -x "$dtc" || fail "missing executable DTC at $dtc"

dtbs=(
	msm8998-red-hydrogenone-tm-pvt.dtb
	msm8998-red-hydrogenone-tm-csp-pvt.dtb
	msm8998-red-hydrogenone-sim-pvt.dtb
	msm8998-red-hydrogenone-jdi-pvt.dtb
)

tmp_dir=$(mktemp -d)
trap 'rm -rf -- "$tmp_dir"' EXIT

for name in "${dtbs[@]}"; do
	dtb="$out/arch/arm64/boot/dts/qcom/$name"
	test -f "$dtb" || fail "missing compiled DTB $dtb"
	dts="$tmp_dir/${name%.dtb}.dts"
	"$dtc" -q -I dtb -O dts -o "$dts" "$dtb"
	block="$tmp_dir/${name%.dtb}.sound-9335"
	awk '
		!in_node && /sound-9335[[:space:]]*\{/ { in_node = 1 }
		in_node {
			print
			opens = gsub(/\{/, "{")
			closes = gsub(/\}/, "}")
			depth += opens - closes
			if (depth == 0) exit
		}
	' "$dts" >"$block"
	test -s "$block" || fail "$name lacks sound-9335"
	grep -Eq 'qcom,wsa-max-devs = <(0x0+|0)>;' "$block" ||
		fail "$name does not disable WSA explicitly"
	grep -Fq 'qcom,tfa98xx-mi2s;' "$block" || fail "$name lacks TFA9894 routing"
	if grep -Eq 'qcom,wsa-devs|qcom,wsa-aux-dev-prefix' "$block"; then
		fail "$name still references WSA881x devices"
	fi
done

printf 'PASS: RED compiled-DTB audio contract\n'
