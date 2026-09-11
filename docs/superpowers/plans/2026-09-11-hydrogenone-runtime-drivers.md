# Hydrogen One Runtime Drivers Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add source-built FPC1020, LM36923H, TFA9894 and Cypress CYTTSP5 support required by the four RED Hydrogen One PVT device trees.

**Architecture:** Port one licensed driver family at a time into the maintained Linux 4.4.302 tree. Keep RED electrical values in the existing stock-derived DTS layer, test each source consumer through the real compiled kernel/DT artifacts, and isolate every family in its own commit and build gate.

**Tech Stack:** Linux 4.4.302, ARM64 Kbuild/Kconfig, Qualcomm MSM8998 DTS, Clang 19, GNU shell verification, `dtc`, `nm`, `strings`, `checkpatch.pl`.

**Spec:** `docs/superpowers/specs/2026-09-11-hydrogenone-runtime-drivers-design.md`

## Global Constraints

- Work only on kernel branch `lineage-22.2`; do not create or publish another kernel branch.
- Keep the base at Linux 4.4.302 and the primary config at `arch/arm64/configs/lineageos_hydrogenone_defconfig`.
- Preserve exact DTB order: TM, TM CSP, SIM, JDI.
- Preserve RED stock GPIOs, regulators, I2C addresses, compatible strings and firmware names.
- SmartPort source, nodes, config symbols and stubs remain excluded.
- Do not modify `device/red/hydrogenone` or `vendor/red/hydrogenone` in this phase.
- Do not claim runtime or boot success from compilation.
- Record exact source repository, branch/ref, commit, file paths and license for every imported family.
- Use built-in drivers (`=y`) because stock `.118` carries the required families built-in and its 4.4.153 module ABI is not reusable.
- All source edits use `apply_patch`; copying an unchanged licensed donor directory is allowed only as a bulk mechanical import, followed by a reviewed diff and provenance entry.
- Every production driver change follows RED, GREEN, REFACTOR: run its artifact contract before the driver exists, confirm the expected missing-driver failure, add the minimal driver, then repeat the same check against a clean build.

---

## Shared build command

Each task uses a new output directory and the following complete toolchain
arguments. Replace only `OUT`, `LOG` and the target list shown by the task.

```bash
set -o pipefail
make -j8 O="$OUT" \
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
  HOSTCC=gcc HOSTCXX=g++ Image.gz-dtb Image.gz modules 2>&1 | tee "$LOG"
status=${PIPESTATUS[0]}
test "$status" -eq 0
```

### Task 1: Reusable compiled-artifact contract gate

**Files:**
- Create: `scripts/hydrogenone/verify-runtime-driver.sh`
- Create: `docs/references/COMMIT_PROVENANCE.csv`

**Interfaces:**
- Consumes: an already configured and compiled Kbuild output directory plus one component name.
- Produces: `verify-runtime-driver.sh <fpc|lm36923h|tfa9894|cyttsp5> <output-dir>` with exit 0 only when the selected driver's real `.config`, `vmlinux` and compiled DT contract exist.

- [ ] **Step 1: Add the executable contract gate before any driver source**

  Implement strict argument validation and component-specific checks. The gate
  must use the selected output's `nm`, `strings` and compiled DTBs rather than
  grepping the driver source. Literal expectations are:

  ```text
  fpc: CONFIG_INPUT_FPC_FINGERPRINT=y; fpc1020_probe; fpc,fpc1020;
       irq, wakeup_enable, wakeup_feature, device_prepare; all three RED supplies
  lm36923h: CONFIG_BACKLIGHT_LM36923H=y; lm36923h_probe; ti,lm36923h;
            LM36923H-BL; hwen-gpio; current-limitation; dual-3d-bkl
  tfa9894: CONFIG_SND_SOC_TFA9894=y; tfa98xx_i2c_probe; nxp,tfa98xx;
           TFA9894 detected; tfa98xx.cnt; tfa98xx_a3d.cnt
  cyttsp5: CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5=y; cyttsp5_i2c_probe;
           cy,cyttsp5_i2c_adapter; cyttsp5_mt; cyttsp5_btn; cyttsp5_proximity
  ```

  The common tail must verify all four DTBs exist, no compiled DTB contains a
  SmartPort marker, and `Image.gz-dtb` exactly equals `Image.gz` followed by
  TM, TM CSP, SIM and JDI.

- [ ] **Step 2: Prove the gate detects the current FPC absence**

  Run:

  ```bash
  scripts/hydrogenone/verify-runtime-driver.sh fpc \
    /tmp/h1-pvt-dts-build-merged-20260911-1
  ```

  Expected: non-zero with `missing CONFIG_INPUT_FPC_FINGERPRINT=y`. This is the
  RED state for the first production change.

- [ ] **Step 3: Initialize the provenance ledger**

  Add a CSV header with exact columns:

  ```text
  target_path,source_repository,source_ref,source_commit,source_path,license,reason,verification
  ```

  Do not add a driver row until that driver has passed its build gate.

### Task 2: FPC1020 platform/TEE resource driver

**Files:**
- Create: `drivers/input/fingerprint/Kconfig`
- Create: `drivers/input/fingerprint/Makefile`
- Create: `drivers/input/fingerprint/fpc1020_platform_tee.c`
- Create: `Documentation/devicetree/bindings/input/fpc1020-platform-tee.txt`
- Modify: `drivers/input/Kconfig`
- Modify: `drivers/input/Makefile`
- Modify: `arch/arm64/configs/lineageos_hydrogenone_defconfig`
- Modify: `docs/references/COMMIT_PROVENANCE.csv`
- Test: `scripts/hydrogenone/verify-runtime-driver.sh`

**Interfaces:**
- Consumes: existing `fpc,fpc1020` platform node, three named pinctrl states, TLMM IRQ/reset GPIOs and `vcc_spi`/`vdd_ana`/`vdd_io` supplies.
- Produces: built-in `CONFIG_INPUT_FPC_FINGERPRINT`, platform driver `fpc1020`, resource power/reset/IRQ control and stock-facing sysfs group.

- [ ] **Step 1: Confirm and record the RED failure**

  Re-run the Task 1 FPC command and retain its non-zero output in
  `/home/surface/los/logs/hydrogenone/kernel_runtime_fpc_red_20260911.log`.

- [ ] **Step 2: Add the minimal licensed FPC driver**

  Base Linux 4.4.302 integration on Razer MSM8998 commit
  `1ea8ada363c839064efd290f81ed131bcae85289`, file
  `drivers/input/fingerprint/fpc1020_tee/fpc1020_platform_tee.c`. Bring the
  regulator-aware `device_prepare()`/`vreg_setup()` behavior from the
  Qualcomm/AOSP GPL-2.0 implementation, preserving only RED property names.

  The driver must:

  ```c
  static const struct of_device_id fpc1020_of_match[] = {
      { .compatible = "fpc,fpc1020" },
      { }
  };
  ```

  It must expose `irq`, `wakeup_enable`, `device_prepare`,
  `regulator_enable`, `hw_reset`, `pinctl_set` and `clk_enable`. Recover the
  stock `wakeup_feature` write semantics through static analysis before adding
  that attribute. Do not implement it as an alias unless binary evidence proves
  equivalence.

- [ ] **Step 3: Wire Kconfig, Makefile and binding**

  `INPUT_FINGERPRINT` is a boolean menu. `INPUT_FPC_FINGERPRINT` depends on it
  and `OF`, selects no unrelated device family, and builds
  `fpc1020_platform_tee.o`. The binding documents only the existing RED
  properties and named states.

- [ ] **Step 4: Enable and normalize the config**

  Add:

  ```text
  CONFIG_INPUT_FINGERPRINT=y
  CONFIG_INPUT_FPC_FINGERPRINT=y
  ```

  Regenerate from a clean output with `lineageos_hydrogenone_defconfig`, run
  `olddefconfig`, then use `savedefconfig` to ensure no unexpected dependency
  appears.

- [ ] **Step 5: Build and run the GREEN gate**

  Build into `/tmp/h1-runtime-fpc-20260911-1`, log to
  `/home/surface/los/logs/hydrogenone/kernel_runtime_fpc_build_20260911.log`,
  then run:

  ```bash
  scripts/hydrogenone/verify-runtime-driver.sh fpc \
    /tmp/h1-runtime-fpc-20260911-1
  ```

  Expected: exit 0. If `wakeup_feature` semantics remain unproved, this task
  stops before GREEN and records the exact static-analysis blocker.

- [ ] **Step 6: Review, provenance and commit**

  Run `scripts/checkpatch.pl --strict` on the new FPC C/binding files, add the
  Razer and Qualcomm/AOSP sources to the provenance row, run
  `git diff --check`, then commit:

  ```bash
  git commit -m "input: add Hydrogen One FPC1020 support"
  ```

### Task 3: LM36923H backlight

**Files:**
- Create: `drivers/video/backlight/lm36923h_bl.c`
- Create: `Documentation/devicetree/bindings/video/backlight/lm36923h.txt`
- Modify: `drivers/video/backlight/Kconfig`
- Modify: `drivers/video/backlight/Makefile`
- Modify: `arch/arm64/configs/lineageos_hydrogenone_defconfig`
- Modify: `docs/references/COMMIT_PROVENANCE.csv`
- Test: `scripts/hydrogenone/verify-runtime-driver.sh`

**Interfaces:**
- Consumes: `ti,lm36923h`, I2C address `0x36`, `hwen-gpio`, optional `current-limitation` and `dual-3d-bkl`.
- Produces: built-in `CONFIG_BACKLIGHT_LM36923H` and LED class device `LM36923H-BL` with 11-bit brightness.

- [ ] **Step 1: Run the LM contract against the FPC build and confirm RED**

  Expected failure: missing `CONFIG_BACKLIGHT_LM36923H=y`.

- [ ] **Step 2: Import the documented TI register model**

  Use upstream GPL-2.0 `drivers/leds/leds-lm3692x.c` at commit
  `4f5f5411f0c14` and the TI LM36923H Rev. B data sheet. Adapt only the kernel
  4.4 APIs, RED compatible/property names and LED-class name. Implement primary
  chip enable, zero brightness, 11-bit LSB/MSB updates and fault-safe probe
  unwind.

- [ ] **Step 3: Gate RED dual-backlight behavior**

  Statically recover the second-chip address and initialization/write sequence
  from the stock Image before enabling `dual-3d-bkl`. If exact behavior is not
  recoverable, return a clear probe diagnostic for that requested mode and stop
  this task; do not silently drive only half of the RED backlight.

- [ ] **Step 4: Wire config, build and verify GREEN**

  Enable `CONFIG_BACKLIGHT_LM36923H=y`, build into
  `/tmp/h1-runtime-lm36923h-20260911-1`, log to
  `kernel_runtime_lm36923h_build_20260911.log`, and run the LM contract gate.

- [ ] **Step 5: Review, provenance and commit**

  Run targeted checkpatch and all DT/Image common checks, then commit:

  ```bash
  git commit -m "video: backlight: add Hydrogen One LM36923H support"
  ```

### Task 4: TFA9894 stereo smart amplifiers

**Files:**
- Create: `sound/soc/codecs/tfa9894/` with the minimal driver, service and register headers required for revision `0x94`
- Modify: `sound/soc/codecs/Kconfig`
- Modify: `sound/soc/codecs/Makefile`
- Modify: `arch/arm64/configs/lineageos_hydrogenone_defconfig`
- Modify: `docs/references/COMMIT_PROVENANCE.csv`
- Test: `scripts/hydrogenone/verify-runtime-driver.sh`

**Interfaces:**
- Consumes: RED TFA nodes at I2C `0x34`/`0x35`, reset/IRQ GPIOs, `tfa98xx,fw-name`, Qualcomm MI2S machine integration.
- Produces: built-in `CONFIG_SND_SOC_TFA9894`, two address-qualified codec DAIs, TFA9894 revision operations and exact `.cnt` firmware requests.

- [ ] **Step 1: Run the TFA contract against the prior build and confirm RED**

  Expected failure: missing `CONFIG_SND_SOC_TFA9894=y`.

- [ ] **Step 2: Mechanically import the licensed Nokia family, then trim**

  Source is `LineageOS/android_kernel_nokia_msm8998`, `lineage-22.2`, commit
  `d18a7df9af20cb6e7d02621ea85a23b83d0e7d8c`, directory
  `sound/soc/codecs/tfa9892`. Preserve copyright/license headers. Retain only
  files referenced by the resulting Makefile and revision `0x94`; remove
  Nokia-only policy and the donor `-Werror` flag.

- [ ] **Step 3: Match the RED ABI**

  Rename the Kconfig family to `SND_SOC_TFA9894`, accept `nxp,tfa98xx` and
  `nxp,tfa9894`, retain revision `0x94`, consume `reset-gpio`, `irq-gpio` and
  `tfa98xx,fw-name`, and ensure DAI names include I2C addresses `8-34` and
  `8-35` as observed in stock.

- [ ] **Step 4: Build and verify GREEN**

  Build into `/tmp/h1-runtime-tfa9894-20260911-1`, log to
  `kernel_runtime_tfa9894_build_20260911.log`, run the TFA gate, and verify the
  kernel contains both exact firmware request names. Do not add the proprietary
  `.cnt` files to Git.

- [ ] **Step 5: Review, provenance and commit**

  Check all imported files and the complete build, then commit:

  ```bash
  git commit -m "ASoC: add Hydrogen One TFA9894 support"
  ```

### Task 5: JDI Cypress CYTTSP5

**Files:**
- Create: `drivers/input/touchscreen/cyttsp5/` with core, devicetree, I2C, MT-B, button and proximity units
- Modify: `drivers/input/touchscreen/Kconfig`
- Modify: `drivers/input/touchscreen/Makefile`
- Modify: `arch/arm64/configs/lineageos_hydrogenone_defconfig`
- Modify: `docs/references/COMMIT_PROVENANCE.csv`
- Test: `scripts/hydrogenone/verify-runtime-driver.sh`

**Interfaces:**
- Consumes: the existing JDI `cy,cyttsp5_i2c_adapter` subtree at `0x24`.
- Produces: built-in Cypress Gen5 I2C core with MT-B, button and proximity input devices, scoped by DT to JDI.

- [ ] **Step 1: Run the Cypress contract against the prior build and confirm RED**

  Expected failure: missing `CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5=y`.

- [ ] **Step 2: Import only the exact GPL AOSP units**

  Use AOSP kernel/msm ref `android-msm-sawshark-3.18-nougat-mr1-wear-release`,
  directory `drivers/input/touchscreen/cyttsp5_cs445a`, whose I2C driver matches
  `cy,cyttsp5_i2c_adapter`. Preserve license headers. Exclude Huawei DSM,
  factory device-access, debug tuner and firmware-loader units unless the core
  has a link-time dependency on them.

- [ ] **Step 3: Port to the target 4.4 APIs and exact JDI topology**

  Enable devicetree support, I2C, MT-B, button and proximity. Do not copy donor
  GPIOs or firmware tables. The driver must consume the child nodes already in
  `msm8998-red-hydrogenone-panel-jdi.dtsi`.

- [ ] **Step 4: Build and verify GREEN**

  Build into `/tmp/h1-runtime-cyttsp5-20260911-1`, log to
  `kernel_runtime_cyttsp5_build_20260911.log`, run the Cypress gate, and repeat
  the four-DTB round-trip. Treat lack of physical JDI hardware as UNVERIFIED,
  not as a build failure.

- [ ] **Step 5: Review, provenance and commit**

  Run targeted checkpatch and commit:

  ```bash
  git commit -m "input: touchscreen: add JDI Cypress CYTTSP5 support"
  ```

### Task 6: Combined regression evidence and publication

**Files:**
- Create: `docs/hydrogenone/runtime-driver-build-evidence.md`
- Modify: `docs/references/COMMIT_PROVENANCE.csv`

**Interfaces:**
- Consumes: the four individual driver commits and their build logs.
- Produces: one clean combined kernel build, hashes, explicit runtime status and a matching remote `lineage-22.2` SHA.

- [ ] **Step 1: Run a fresh combined build**

  Configure and build in `/tmp/h1-runtime-drivers-final-20260911-1`; save the
  complete output at
  `/home/surface/los/logs/hydrogenone/kernel_runtime_drivers_final_20260911.log`.
  Run all four component gates.

- [ ] **Step 2: Repeat DTB, appended-image and boot-budget checks**

  Round-trip all four DTBs, compare the exact appended byte stream, reject all
  SmartPort markers, and recalculate the 64 MiB boot budget using the new
  `Image.gz-dtb` plus the measured 9,466,573-byte ramdisk and 1,320-byte
  signature allowance.

- [ ] **Step 3: Verify repository isolation**

  Require a clean kernel status after the evidence commit. Confirm the device
  tree retains only its pre-existing `BoardConfig.mk` modification/backup and
  the vendor tree has no new change.

- [ ] **Step 4: Write evidence and commit**

  Record exact inputs, source commits, build commands, exit codes, artifact
  hashes, warnings, firmware packaging gap and every physical UNVERIFIED item.
  Commit:

  ```bash
  git commit -m "docs: record Hydrogen One runtime driver build"
  ```

- [ ] **Step 5: Final verification and push**

  Re-run the clean build/tests after the evidence commit, verify branch and
  origin, then push normally:

  ```bash
  git push origin lineage-22.2
  ```

  Compare `git rev-parse HEAD` with
  `git ls-remote --heads origin refs/heads/lineage-22.2`. Never force-push.
