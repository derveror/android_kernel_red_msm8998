# RED Hydrogen One PVT DTB build evidence

Date: 2026-09-11

## Scope

This record covers the static Linux 4.4.302 kernel/DTS stage for four RED
Hydrogen One production/PVT variants. It does not claim that an image was
flashed or that the phone boots. The physical verification stage was
explicitly skipped.

The implementation worktree started at
`818278ea069f3a3caa953e99c87a9d10e98d705e` and the source revision tested
below is `58befc8933eb30cacb5d5f60570f3c2a066cb8bd`.

## Stock evidence

The supplied `.118` boot image is the authority for RED-specific hardware
values:

| Input | Bytes | SHA-256 |
|---|---:|---|
| `boot.img` | 46,495,016 | `8e120a2920f5d4eec65cb5929d31fe271738af85b218d5adb96035eb28806af6` |
| `stock.dtb.19` — TM PVT, stock ordinal 37 | 399,342 | `60eab26d95ef200a27aee03edf6e8edafd86a838ffe5c01c2c60d8d3fff1777e` |
| `stock.dtb.10` — TM CSP PVT, stock ordinal 46 | 399,381 | `1741bc052d6b3c20633310fb9ddfa0e5de430691fbe33da1c958c33d2d974064` |
| `stock.dtb.4` — SIM PVT, stock ordinal 52 | 399,314 | `e26dc14b9358d130092cefe84a0358f8548412930dea1ae08994abeb4bf6c0d6` |
| `stock.dtb.1` — JDI PVT, stock ordinal 55 | 399,555 | `b839b884276f9b404a9608877605c1748aa726a59733f79910421050b941944d` |

The relative physical order used by the new image is TM, TM CSP, SIM, JDI.
All four source roots preserve:

- `qcom,msm-id = <0x124 0x20001>`;
- `qcom,board-id = <8 0 1 0>`;
- `fih,hw-id = <4 4 0>`;
- display IDs TM `<0x02>`, TM CSP `<0x0e>`, SIM `<0x7f>`, JDI `<0x64>`.

## Source layout and boundaries

The four top-level DTS files retain the maintained Qualcomm MSM8998 v2.1 MTP
include hierarchy. RED-wide data is in
`msm8998-red-hydrogenone-common.dtsi`, PVT-stage data is in
`msm8998-red-hydrogenone-pvt.dtsi`, and the selected display/touch differences
are isolated in four panel DTSIs.

The common layer contains stock-derived reserved memory, ramoops, PMIC thermal
channels, the 4510 mAh battery profile, fingerprint wiring, NFC eSE power GPIO,
TFA audio routing, camera/EEPROM supplies and pinctrl, Synaptics touch and the
LM36923H node. The active panel payload and command sequences were compared
against the matching stock DTB after compilation and matched exactly, apart
from dynamically assigned phandles.

The proprietary rear SmartPort is deliberately absent. No
`cloudminds,smartport`, `cmti,smartport`, `cm,smartp-req`, accessory SmartPort
UART or SmartPort pinctrl node is present. This exclusion does not remove the
ordinary DWC3/USB-C PHY, SMB2/FG charging, UFS or WCN3990 Bluetooth paths.

## Reproducible build

The clean output directory was
`/tmp/h1-pvt-dts-build-final-20260911-1`. Configuration was regenerated with
`lineageos_hydrogenone_defconfig`, followed by:

```text
make -j8 O=/tmp/h1-pvt-dts-build-final-20260911-1 \
  ARCH=arm64 LLVM=1 LLVM_IAS=1 CLANG_TRIPLE=aarch64-linux-gnu- \
  CC=.../clang-r530567/bin/clang LD=.../clang-r530567/bin/ld.lld \
  AR=.../clang-r530567/bin/llvm-ar NM=.../clang-r530567/bin/llvm-nm \
  OBJCOPY=.../clang-r530567/bin/llvm-objcopy \
  OBJDUMP=.../clang-r530567/bin/llvm-objdump \
  STRIP=.../clang-r530567/bin/llvm-strip \
  CROSS_COMPILE=.../aarch64-linux-android-4.9/bin/aarch64-linux-android- \
  CROSS_COMPILE_ARM32=.../arm-linux-androideabi-4.9/bin/arm-linux-androideabi- \
  HOSTCC=gcc HOSTCXX=g++ Image.gz-dtb Image.gz modules
```

The build exited successfully, produced kernel release `4.4.302+`, and built
10 modules. The regenerated configuration contains exactly:

```text
CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE=y
CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES="qcom/msm8998-red-hydrogenone-tm-pvt qcom/msm8998-red-hydrogenone-tm-csp-pvt qcom/msm8998-red-hydrogenone-sim-pvt qcom/msm8998-red-hydrogenone-jdi-pvt"
```

## Static verification

- All four standalone DTBs compiled successfully.
- Each DTB completed a DTB → sorted DTS → DTB → sorted DTS round trip with
  identical first and second sorted DTS representations.
- Root models and all selection IDs match the four stock inputs.
- Active panel properties, timings, reset sequences, DSC data and DSI command
  payloads match their stock counterparts.
- Common camera/EEPROM, fingerprint, Synaptics, LM36923H, TFA, battery and
  ramoops nodes match the stock values after phandle-to-path normalization.
- TM/TM CSP retain Synaptics touch; SIM disables its touch I2C bus; JDI uses
  the stock Cypress subtree.
- Each output contains `snps,dwc3`, `qcom,usb-ssphy-qmp-v2`,
  `qcom,qpnp-smb2`, `qcom,fg-gen3`, `qca,wcn3990` and `ufshc@1da4000`.
- Each output is free of all SmartPort markers listed above.
- A byte-stream comparison proved that `Image.gz-dtb` is exactly `Image.gz`
  followed by the TM, TM CSP, SIM and JDI DTBs, with no fifth or generic DTB.
- `git diff --check` passed.

Round-trip DTC emits 30 legacy `unit_address_vs_reg` diagnostics per output.
Twenty-eight are also emitted by the supplied stock TM tree. The remaining two
come from inherited Qualcomm `sound-9335/msm_cdc_pinctrl@67` and `@68` nodes;
none is introduced by a RED hardware value. The full kernel build also retains
three pre-existing diagnostics: the MSM HDMI Kconfig dependency warning, the
NVMe conditional-precedence warning, and the UFS test `SECTOR_SIZE` macro
redefinition. There is no new build blocker.

## Artifacts

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `.config` | 140,574 | `a87876316f656b3ee72d0e21fd4721c7df04f5bf32f9e77b731dd266ad0e9c7f` |
| TM PVT DTB | 393,858 | `2fa940e87cba7b859a27d0d8f02978dfd5f10c983bddb98e9b02e3f150d90074` |
| TM CSP PVT DTB | 393,897 | `90ac4c615418da449f356856fd0de21620f3dc4c1d9d396c99da86d786b36d50` |
| SIM PVT DTB | 393,472 | `5047d81796e62bfd5f8239d36ed698f2e74999d09a988db9e8cd77467eabd90d` |
| JDI PVT DTB | 393,919 | `da74adc9f963dfd170cc24e2d5306ea97ca1d5eda3e515945c8c7b3bb2b73447` |
| `Image.gz` | 15,185,545 | `50684614383656ff4a7c2c15853618bb704ebd493f660cfc7a8d399071bd6225` |
| `Image.gz-dtb` | 16,760,691 | `16021b5dca7a5caf767317b673aa369f7a605f198446b25e64c6a2562c0a2ba8` |
| `vmlinux` | 315,709,200 | `5d9029b96176f76bc86f7a85e19008788d38bbb132d32d644bab0ff5624fcc05` |
| `System.map` | 6,796,458 | `92c4eafb74ff53cab3767e8aee75861b9efc3b82ee45d187ae8827809ce5e256` |
| `Module.symvers` | 580,122 | `eccd3993c62708092fc44db693fa272d7eb1b41219e70911af519a1fea439ad0` |

## Boot partition budget

Using the measured `Image.gz-dtb`, the stock 9,466,573-byte ramdisk, 4,096-byte
page size and 1,320-byte signature allowance:

```text
4096 + align_up(16760691, 4096) + align_up(9466573, 4096) + 1320
= 26236200 bytes
```

This is 39.09% of the 67,108,864-byte boot partition and leaves 40,872,664
bytes (60.91%). It is a conservative size projection, not a generated or
boot-tested `boot.img`.

## Isolation and remaining physical work

No device or vendor file was changed by this stage. The device repository
retains its pre-existing modified `BoardConfig.mk` and untracked
`BoardConfig.mk.backup.20260908_140629`; the vendor repository remains clean.

Runtime probing remains unverified. In particular, DTS presence alone does
not prove display output, touch, cameras, speakers, fingerprint, charging,
Bluetooth or radio operation. Exact runtime support for RED-only LM36923H,
TFA98xx, FPC1020 and JDI Cypress components must be confirmed against available
driver sources and then on physical hardware in a later stage.
