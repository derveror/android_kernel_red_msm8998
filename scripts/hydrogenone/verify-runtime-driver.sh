#!/bin/bash

set -euo pipefail

usage() {
	printf 'usage: %s <fpc|lm36923h|tfa9894|cyttsp5> <output-dir>\n' "$0" >&2
	exit 2
}

fail() {
	printf 'FAIL: %s\n' "$*" >&2
	exit 1
}

test "$#" -eq 2 || usage

component=$1
out=${2%/}

case "$component" in
	fpc|lm36923h|tfa9894|cyttsp5)
		;;
	*)
		usage
		;;
esac

config="$out/.config"
vmlinux="$out/vmlinux"
dtb_dir="$out/arch/arm64/boot/dts/qcom"
image="$out/arch/arm64/boot/Image.gz"
image_dtb="$out/arch/arm64/boot/Image.gz-dtb"
dtbs=(
	"$dtb_dir/msm8998-red-hydrogenone-tm-pvt.dtb"
	"$dtb_dir/msm8998-red-hydrogenone-tm-csp-pvt.dtb"
	"$dtb_dir/msm8998-red-hydrogenone-sim-pvt.dtb"
	"$dtb_dir/msm8998-red-hydrogenone-jdi-pvt.dtb"
)

for artifact in "$config" "$vmlinux" "$image" "$image_dtb" "${dtbs[@]}"; do
	test -f "$artifact" || fail "missing artifact $artifact"
done

if test -n "${LLVM_NM:-}"; then
	nm_bin=$LLVM_NM
elif command -v llvm-nm >/dev/null 2>&1; then
	nm_bin=$(command -v llvm-nm)
elif command -v llvm-nm-19 >/dev/null 2>&1; then
	nm_bin=$(command -v llvm-nm-19)
elif command -v nm >/dev/null 2>&1; then
	nm_bin=$(command -v nm)
else
	fail "no nm implementation found"
fi

tmp_dir=$(mktemp -d)
trap 'rm -rf -- "$tmp_dir"' EXIT

"$nm_bin" -n "$vmlinux" >"$tmp_dir/vmlinux.nm"
strings -a -n 3 "$vmlinux" >"$tmp_dir/vmlinux.strings"
for dtb in "${dtbs[@]}"; do
	strings -a -n 3 "$dtb"
done >"$tmp_dir/all-dtbs.strings"
strings -a -n 3 "${dtbs[3]}" >"$tmp_dir/jdi-dtb.strings"

require_config() {
	local setting=$1
	grep -Fqx -- "$setting" "$config" || fail "missing $setting"
}

require_symbol() {
	local symbol=$1
	grep -Eq "[[:space:]][[:alpha:]]([[:space:]])${symbol}$" \
		"$tmp_dir/vmlinux.nm" || fail "missing symbol $symbol"
}

reject_symbol() {
	local symbol=$1
	if grep -Eq "[[:space:]][[:alpha:]]([[:space:]])${symbol}$" \
			"$tmp_dir/vmlinux.nm"; then
		fail "unexpected symbol $symbol"
	fi
}

require_kernel_string() {
	local value=$1
	grep -Fq -- "$value" "$tmp_dir/vmlinux.strings" || \
		fail "missing kernel string $value"
}

require_dtb_string() {
	local value=$1
	grep -Fqx -- "$value" "$tmp_dir/all-dtbs.strings" || \
		fail "missing DTB string $value"
}

case "$component" in
	fpc)
		require_config CONFIG_INPUT_FPC_FINGERPRINT=y
		require_symbol fpc1020_probe
		require_kernel_string fpc,fpc1020
		for attribute in irq wakeup_enable wakeup_feature device_prepare; do
			require_kernel_string "$attribute"
		done
		for supply in vcc_spi-supply vdd_ana-supply vdd_io-supply; do
			require_dtb_string "$supply"
		done
		;;
	lm36923h)
		require_config CONFIG_BACKLIGHT_LM36923H=y
		require_symbol lm36923h_probe
		require_kernel_string ti,lm36923h
		require_kernel_string LM36923H-BL
		for property in hwen-gpio current-limitation dual-3d-bkl; do
			require_dtb_string "$property"
		done
		;;
	tfa9894)
		require_config CONFIG_SND_SOC_TFA9894=y
		require_symbol tfa98xx_i2c_probe
		require_kernel_string nxp,tfa98xx
		require_kernel_string 'TFA9894 detected'
		require_kernel_string tfa98xx.cnt
		require_kernel_string tfa98xx-aif-8-34
		require_kernel_string tfa98xx-aif-8-35
		require_kernel_string qcom,tfa98xx-mi2s
		require_dtb_string tfa98xx_a3d.cnt
		require_dtb_string tfa98xx,fw-name
		;;
	cyttsp5)
		require_config CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5=y
		for feature in DEVICETREE_SUPPORT I2C MT_B BUTTON PROXIMITY; do
			require_config "CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5_${feature}=y"
		done
		require_symbol cyttsp5_i2c_probe
		for property in cy,cyttsp5_i2c_adapter cy,bus-reg-name \
				cy,irq_gpio cy,rst_gpio cy,name; do
			require_kernel_string "$property"
		done
		reject_symbol cyttsp5_loader_probe
		reject_symbol cyttsp5_procfs_create
		grep -Fq -- cy,cyttsp5_i2c_adapter "$tmp_dir/jdi-dtb.strings" || \
			fail "JDI DTB lacks Cypress I2C adapter"
		for dtb in "${dtbs[@]:0:3}"; do
			if strings -a -n 3 "$dtb" | grep -Fq -- cy,cyttsp5_i2c_adapter; then
				fail "non-JDI DTB contains Cypress I2C adapter: $dtb"
			fi
		done
		for child in cyttsp5_mt cyttsp5_btn cyttsp5_proximity; do
			grep -Fqx -- "$child" "$tmp_dir/jdi-dtb.strings" || \
				fail "missing JDI DTB string $child"
		done
		;;
esac

if grep -Eiq 'smart[-_]?port|red,smart' "$tmp_dir/all-dtbs.strings"; then
	fail "compiled DTB contains a SmartPort marker"
fi

cmp -s "$image_dtb" <(cat "$image" "${dtbs[@]}") || \
	fail "Image.gz-dtb does not contain the exact TM/TM-CSP/SIM/JDI sequence"

printf 'PASS: %s runtime-driver artifact contract\n' "$component"
