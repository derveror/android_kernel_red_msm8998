# RED Hydrogen One runtime-driver build evidence

Date: 2026-09-11

## Scope and status

This record covers the static Linux 4.4.302 integration of the source-built
FPC1020 fingerprint resource driver, LM36923H backlight, stereo TFA9894 smart
amplifiers and the JDI Cypress CYTTSP5 touch stack. All four production/PVT
device variants remain in scope: TM, TM CSP, SIM and JDI.

The combined source revision tested before this evidence-only commit is
`925394978ead9dcaf715c67a5dd54ccfef7f47e0`. Compilation and static artifact
contracts pass. No image was flashed and no boot or runtime hardware result is
claimed.

SmartPort means the proprietary RED/CloudMinds rear accessory interface. Its
nodes, compatible strings, config symbols and driver stubs remain excluded.
This does not remove ordinary USB-C/DWC3, charging, UFS or Bluetooth support.
Leia/display stays in scope.

## Authoritative inputs

| Input | Bytes | SHA-256 |
|---|---:|---|
| Master instruction `hydrogenone_lineageos22_2_kernel_tree_master_instruction_ru.md` | 52,236 | `d5160929dd8e7c7b9369447545df881c38a347473c1f1fc410a75e889e838984` |
| Stock `.118` `boot.img` | 46,495,016 | `8e120a2920f5d4eec65cb5929d31fe271738af85b218d5adb96035eb28806af6` |
| Stock `.118` kernel Image used for binary analysis | — | `22872b8423e41940cd2176d589f98ebc6e625859cffef945f77ea486fb6edf56` |

The stock boot image remains the authority for RED-specific GPIOs, supplies,
I2C addresses, compatible strings, firmware names, board/display IDs and DTB
ordering. The measured stock ramdisk input for the size projection is
9,466,573 bytes.

## Driver-source provenance

| Family | Source and exact revision | License | RED use |
|---|---|---|---|
| FPC1020 | LineageOS Razer MSM8998 `lineage-22.2`, `1ea8ada363c839064efd290f81ed131bcae85289`; AOSP kernel/msm `b6410c3f75259c0ca2df45a01838d2e46272723e` | GPL-2.0 / GPL-2.0-only | Platform/TEE resources, regulators, IRQ/wake and stock sysfs ABI |
| LM36923H | AOSP kernel/common `4f5f5411f0c14ac0b61d5e6a77d996dd3d5b5fd3` | GPL-2.0-only | TI register model, RED primary `0x36` and stock-recovered secondary `0x37` behavior |
| TFA9894 | LineageOS Nokia MSM8998 `lineage-22.2`, `d18a7df9af20cb6e7d02621ea85a23b83d0e7d8c` | Apache-2.0 | TFA98xx service, revision `0x94`, two address-qualified codec DAIs and Primary MI2S RX routing |
| CYTTSP5 | AOSP kernel/msm `android-msm-sawshark-3.18-nougat-mr1-wear-release`, `22c580f7fb449994cf60024c9fde9e54e0592f16` | GPL-2.0-only | JDI-only devicetree/I2C/MT-B/button/proximity stack |

The machine-driver TFA component/DAI names and firmware-property behavior were
recovered from the stock `.118` kernel. Full per-path provenance is retained in
`docs/references/COMMIT_PROVENANCE.csv`.

## Reproducible combined build

The fresh output directory was
`/tmp/h1-runtime-drivers-final-20260911-1`. Configuration was regenerated from
`lineageos_hydrogenone_defconfig`. The build used Android Clang 19.0.0
`r530567` (`97a699bf4812a18fb657c2779f5296a4ab2694d2`) and this command:

```text
make -j8 O=/tmp/h1-runtime-drivers-final-20260911-1 \
  ARCH=arm64 LLVM=1 LLVM_IAS=1 CLANG_TRIPLE=aarch64-linux-gnu- \
  CC=/home/surface/los/prebuilts/clang/host/linux-x86/clang-r530567/bin/clang \
  LD=/home/surface/los/prebuilts/clang/host/linux-x86/clang-r530567/bin/ld.lld \
  AR=/home/surface/los/prebuilts/clang/host/linux-x86/clang-r530567/bin/llvm-ar \
  NM=/home/surface/los/prebuilts/clang/host/linux-x86/clang-r530567/bin/llvm-nm \
  OBJCOPY=/home/surface/los/prebuilts/clang/host/linux-x86/clang-r530567/bin/llvm-objcopy \
  OBJDUMP=/home/surface/los/prebuilts/clang/host/linux-x86/clang-r530567/bin/llvm-objdump \
  STRIP=/home/surface/los/prebuilts/clang/host/linux-x86/clang-r530567/bin/llvm-strip \
  CROSS_COMPILE=/home/surface/los/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android- \
  CROSS_COMPILE_ARM32=/home/surface/los/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.9/bin/arm-linux-androideabi- \
  HOSTCC=gcc HOSTCXX=g++ Image.gz-dtb Image.gz modules
```

The configuration and build both exited with status 0, produced kernel release
`4.4.302+`, and built 10 modules. The complete 179,118-byte log is
`/home/surface/los/logs/hydrogenone/kernel_runtime_drivers_final_20260911.log`
(SHA-256
`cb4aed4176e58841a141b0aa5f503c23ff804f6dbb4557ccc22c02997972ff78`).

The compiled configuration enables all four families built-in and contains the
exact four-name appended-DTB list in this order: TM, TM CSP, SIM, JDI. A
regenerated `savedefconfig` is byte-identical to the checked-in defconfig.

## Static verification

All checks below exited with status 0:

- FPC1020, LM36923H, TFA9894 and CYTTSP5 compiled-artifact gates inspected the
  real `.config`, `vmlinux`, compiled DTBs and appended image.
- Each of the four DTBs completed DTB to sorted DTS to DTB to sorted DTS with
  identical first and second representations.
- `Image.gz-dtb` is byte-for-byte `Image.gz` followed by exactly TM, TM CSP,
  SIM and JDI, with no generic Qualcomm or fifth DTB.
- All compiled DTBs and the implementation/config sources are free of the RED
  and CloudMinds SmartPort identifiers.
- Cypress is present only in the JDI DTB. Its firmware loader, Huawei DSM and
  factory procfs paths are not built.
- `git diff --check` passes.

The round-trip log is
`/home/surface/los/logs/hydrogenone/kernel_runtime_drivers_dtb_roundtrip_20260911.log`
(36,828 bytes, SHA-256
`d75e60c4feb35480f19b25f11c01b95b36eb438d10e38aff792ecd913526a6c9`).
The legacy tree emits 30 `unit_address_vs_reg` diagnostics per DTC pass; the
three-pass round trip over four DTBs therefore records 360. These diagnostics
were already classified in the PVT DTB evidence and do not change a RED
hardware value.

The full build log contains 16 lines marked `warning:`: four repeated inherited
MSM HDMI Kconfig dependency diagnostics, nine enum-conversion diagnostics in
the imported NXP TFA service, one inherited NVMe precedence diagnostic, and
two repeated inherited UFS test `SECTOR_SIZE` redefinitions. The TFA enum
types have distinct numeric spaces, so their conversions were not hidden with
casts. No warning is a build error.

## Artifacts

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `.config` | 140,952 | `193dc7a347c71df37db473b33ab1d1e23fa4db22f91c61291a54c4984cdf8a45` |
| `vmlinux` | 317,047,080 | `17e0752530ae24bdf60d14c267c80708dd7c72de99d7c82a72db8f3022367f18` |
| `System.map` | 6,839,205 | `de3814e0a7e260e20eb032afdd66bab335a23da96ebb9f41d06ce9b74f18a2bb` |
| `Module.symvers` | 580,534 | `a82d566dce4bf1ed693224fa38daab9280ad61d1b5c59e473266ad3324e3f91e` |
| `Image.gz` | 15,300,605 | `4c5f6938f1da2586d1e75fa39246d99462d3b503091501023d0fb5916ab34f1c` |
| `Image.gz-dtb` | 16,875,751 | `0e776b60a2fe14e2fd09d410d613417d5fd0eed2825a59db7cf698dd46dfd149` |
| TM PVT DTB | 393,858 | `2fa940e87cba7b859a27d0d8f02978dfd5f10c983bddb98e9b02e3f150d90074` |
| TM CSP PVT DTB | 393,897 | `90ac4c615418da449f356856fd0de21620f3dc4c1d9d396c99da86d786b36d50` |
| SIM PVT DTB | 393,472 | `5047d81796e62bfd5f8239d36ed698f2e74999d09a988db9e8cd77467eabd90d` |
| JDI PVT DTB | 393,919 | `da74adc9f963dfd170cc24e2d5306ea97ca1d5eda3e515945c8c7b3bb2b73447` |

## Boot-partition budget

Using the measured `Image.gz-dtb`, stock 9,466,573-byte ramdisk, 4,096-byte
page size and 1,320-byte signature allowance:

```text
4096 + align_up(16875751, 4096) + align_up(9466573, 4096) + 1320
= 4096 + 16879616 + 9469952 + 1320
= 26354984 bytes
```

The projection uses 39.27% of the 67,108,864-byte boot partition and leaves
40,753,880 bytes (60.73%). It is a size calculation, not a generated or
boot-tested `boot.img`.

## Firmware gap and physical UNVERIFIED work

The driver requests the stock TFA container names `tfa98xx.cnt` and
`tfa98xx_a3d.cnt`, but neither file is present in the current kernel, device or
vendor tree. Packaging those firmware containers remains a later device/vendor
task; those repositories were intentionally not modified in this phase.

The following remains explicitly **UNVERIFIED** until controlled tests on
physical hardware:

- boot and early kernel logs on TM, TM CSP, SIM and JDI;
- FPC probe, regulators, reset, IRQ/wake, suspend and Android fingerprint use;
- LM36923H brightness, primary/secondary synchronization and dual-3D behavior;
- TFA I2C probe, firmware loading, DSP operation, stereo routing and speakers;
- JDI Cypress power/reset/IRQ, multitouch, buttons, proximity and suspend;
- ordinary USB-C/data, charging, UFS and Bluetooth runtime behavior;
- Leia/display output and the remaining camera, NFC, radio and sensor paths.

At evidence-writing time the kernel tree contains only these evidence and
provenance documentation changes. `device/red/hydrogenone` retains its
pre-existing modified `BoardConfig.mk` and untracked
`BoardConfig.mk.backup.20260908_140629`;
`vendor/red/hydrogenone` is clean.
