# Hydrogen One PVT Device Trees Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build four layered RED Hydrogen One PVT DTBs on the Linux 4.4.302 MSM8998 base and package exactly those DTBs into a verified `Image.gz-dtb`.

**Architecture:** Retain the existing Qualcomm MSM8998 v2.1 MTP include hierarchy, place RED-wide data in one common DTSI, PVT-stage data in one PVT DTSI, and isolate the four display variants in small panel DTSIs and top-level DTS files. Use the supplied stock `.118` DTBs only as RED hardware evidence, omit SmartPort, and keep the generic Qualcomm platform implementation from the maintained 4.4.302 base.

**Tech Stack:** Linux 4.4 Kbuild/Kconfig, DTS/DTSI, in-tree DTC tools, Android Clang/LLD 19, AArch64/ARM32 GCC 4.9 cross tools, shell/Python read-only artifact checks.

**Spec:** `docs/superpowers/specs/2026-09-11-hydrogenone-pvt-device-tree-design.md`

## Global Constraints

- Target branch is `lineage-22.2` in `/home/surface/los/kernel/red/msm8998`; baseline is Linux `4.4.302`.
- Support only TM PVT, TM CSP PVT, SIM PVT and JDI PVT in their relative physical stock order.
- Preserve `qcom,msm-id = <0x124 0x20001>`, `qcom,board-id = <8 0 1 0>` and `fih,hw-id = <4 4 0>`.
- Preserve display IDs JDI `<0x64>`, SIM `<0x7f>`, TM CSP `<0x0e>` and TM `<0x02>`.
- Do not add `cloudminds,smartport`, `cm,smartp-req` or external SmartPort power controls.
- Do not remove normal DWC3/USB-C, Bluetooth/WCN3990, SMB2/SMB138x, fuel-gauge or internal display support.
- Do not copy hardware values from non-RED donor devices.
- Do not modify `/home/surface/los/device/red/hydrogenone` or `/home/surface/los/vendor/red/hydrogenone`.
- Treat the boot partition limit as exactly `67108864` bytes; stock page size is `4096`, stock ramdisk size is `9466573`, and the stock signature allowance is `1320` bytes.
- Static success is not a boot claim; the physical stage remains unverified.

---

### Task 1: Materialize the stock-to-QCOM evidence set

**Files:**
- Read: `/tmp/h1-stage2-final.U300ku/dtbs/stock.dtb.1`
- Read: `/tmp/h1-stage2-final.U300ku/dtbs/stock.dtb.4`
- Read: `/tmp/h1-stage2-final.U300ku/dtbs/stock.dtb.10`
- Read: `/tmp/h1-stage2-final.U300ku/dtbs/stock.dtb.19`
- Read: `arch/arm/boot/dts/qcom/msm8998-v2.1-mtp.dts`
- Generate outside Git: `/tmp/h1-pvt-dts-evidence/`

**Interfaces:**
- Consumes: supplied stock boot DTBs and the existing Qualcomm v2.1 MTP source.
- Produces: four decompiled PVT trees, one decompiled generic tree, cross-PVT diffs and generic-to-RED diffs used by Tasks 2–4.

- [ ] **Step 1: Create an isolated execution worktree and clean evidence/output directories**

Use `superpowers:using-git-worktrees`, then define explicit task paths:

```bash
H1_TREE=/home/surface/los/.worktrees/kernel-red-msm8998-hydrogenone-pvt-dts
H1_OUT=/tmp/h1-pvt-dts-build
H1_EVIDENCE=/tmp/h1-pvt-dts-evidence
H1_STOCK=/tmp/h1-stage2-final.U300ku/dtbs
```

Create `H1_OUT` and `H1_EVIDENCE` as fresh temporary directories. Never remove or overwrite the main checkout, device tree or vendor tree.

- [ ] **Step 2: Build the generic MSM8998 v2.1 MTP comparison DTB**

Run `lineageos_hydrogenone_defconfig`, then build `qcom/msm8998-v2.1-mtp.dtb` with the exact Clang/LLD/GCC variables listed in Task 6. Expected: the generic DTB and the in-tree `dtc` binary exist under `H1_OUT`.

- [ ] **Step 3: Decompile the generic and four stock inputs**

Use `H1_OUT/scripts/dtc/dtc -I dtb -O dts -s` to produce:

```text
generic-msm8998-v2.1-mtp.dts
jdi-pvt-stock.dts
sim-pvt-stock.dts
tm-csp-pvt-stock.dts
tm-pvt-stock.dts
```

Expected root models are respectively `JDI PVT`, `SIM PVT`, `TM CSP PVT`, and `TM PVT` for the four stock files.

- [ ] **Step 4: Generate two classes of sorted diffs**

Use `scripts/dtc/dtx_diff` for each stock DTB versus the built generic DTB, and for JDI versus each other PVT DTB. Store all output only under `H1_EVIDENCE`. Expected: the cross-PVT diffs identify panel/display changes while the generic-to-stock diffs identify RED board overrides.

- [ ] **Step 5: Classify every changed node before source edits**

Create a temporary three-column manifest: node/property path, classification (`common`, `pvt`, `panel`, `smartport-skip`, `unresolved`), and stock evidence files. Do not proceed while any boot-critical difference is classified `unresolved`; record and stop instead of guessing.

### Task 2: Add four buildable layered PVT roots and exact packaging selection

**Files:**
- Create: `arch/arm/boot/dts/qcom/msm8998-red-hydrogenone-common.dtsi`
- Create: `arch/arm/boot/dts/qcom/msm8998-red-hydrogenone-pvt.dtsi`
- Create: `arch/arm/boot/dts/qcom/msm8998-red-hydrogenone-panel-jdi.dtsi`
- Create: `arch/arm/boot/dts/qcom/msm8998-red-hydrogenone-panel-sim.dtsi`
- Create: `arch/arm/boot/dts/qcom/msm8998-red-hydrogenone-panel-tm-csp.dtsi`
- Create: `arch/arm/boot/dts/qcom/msm8998-red-hydrogenone-panel-tm.dtsi`
- Create: `arch/arm/boot/dts/qcom/msm8998-red-hydrogenone-jdi-pvt.dts`
- Create: `arch/arm/boot/dts/qcom/msm8998-red-hydrogenone-sim-pvt.dts`
- Create: `arch/arm/boot/dts/qcom/msm8998-red-hydrogenone-tm-csp-pvt.dts`
- Create: `arch/arm/boot/dts/qcom/msm8998-red-hydrogenone-tm-pvt.dts`
- Modify: `arch/arm/boot/dts/qcom/Makefile`
- Modify: `arch/arm64/configs/lineageos_hydrogenone_defconfig`

**Interfaces:**
- Consumes: the layer classification from Task 1 and existing Qualcomm DTSIs.
- Produces: four independently compilable PVT DTBs and an exact four-name appended-DTB configuration.

- [ ] **Step 1: Run the missing-target test**

Run Kbuild for `qcom/msm8998-red-hydrogenone-jdi-pvt.dtb`. Expected before implementation: FAIL because the target and source do not exist.

- [ ] **Step 2: Add the layer skeletons and exact root metadata**

Each top-level DTS includes, in this order:

```dts
/dts-v1/;

#include "msm8998-v2.1.dtsi"
#include "msm8998-mdss-panels.dtsi"
#include "msm8998-mtp.dtsi"
#include "msm8998-red-hydrogenone-common.dtsi"
#include "msm8998-red-hydrogenone-pvt.dtsi"
```

The final include is respectively
`msm8998-red-hydrogenone-panel-jdi.dtsi`,
`msm8998-red-hydrogenone-panel-sim.dtsi`,
`msm8998-red-hydrogenone-panel-tm-csp.dtsi` or
`msm8998-red-hydrogenone-panel-tm.dtsi` in the matching top-level file.

Each root defines its exact stock model, common compatible/board/MSM/FIH IDs and one of the four display IDs. Keep the shared and panel layers empty except for SPDX/copyright comments until their evidence-backed properties are added in Tasks 3 and 4.

- [ ] **Step 3: Register only the four new DTB targets**

Add the four files to the `CONFIG_ARCH_MSM8998` DTB list in this physical stock order:

```make
msm8998-red-hydrogenone-tm-pvt.dtb
msm8998-red-hydrogenone-tm-csp-pvt.dtb
msm8998-red-hydrogenone-sim-pvt.dtb
msm8998-red-hydrogenone-jdi-pvt.dtb
```

- [ ] **Step 4: Configure exact appended names**

Set one defconfig value with names without the `.dtb` suffix:

```text
CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES="qcom/msm8998-red-hydrogenone-tm-pvt qcom/msm8998-red-hydrogenone-tm-csp-pvt qcom/msm8998-red-hydrogenone-sim-pvt qcom/msm8998-red-hydrogenone-jdi-pvt"
```

- [ ] **Step 5: Build and inspect all four skeleton DTBs**

Expected: all four compile. Decompiled roots contain exact IDs and models; the four DTB names appear exactly once and in the intended order in both Makefile and defconfig.

- [ ] **Step 6: Commit the buildable root/layout milestone**

```bash
git add arch/arm/boot/dts/qcom arch/arm64/configs/lineageos_hydrogenone_defconfig
git commit -m "arm64: dts: add Hydrogen One PVT variants"
```

### Task 3: Reconstruct the common RED and PVT hardware layers

**Files:**
- Modify: `arch/arm/boot/dts/qcom/msm8998-red-hydrogenone-common.dtsi`
- Modify: `arch/arm/boot/dts/qcom/msm8998-red-hydrogenone-pvt.dtsi`

**Interfaces:**
- Consumes: Task 1 manifest rows classified `common` or `pvt`.
- Produces: shared RED production hardware overrides consumed by every panel variant.

- [ ] **Step 1: Write failing property checks for one group at a time**

For each group below, decompile all four current DTBs and assert the exact stock property values and node status. Expected before adding a missing group: at least one assertion fails.

Process groups in dependency order:

1. aliases/chosen/reserved-memory and board identification helpers;
2. PMICs, regulators, clocks and GPIO/pinctrl states;
3. UFS/storage and boot-device dependencies;
4. DWC3, QUSB/QMP PHY and USB-PD;
5. QPNP SMB2, SMB138x, fuel gauge and battery data;
6. Bluetooth power/WCN3990 and shared serial/slimbus data;
7. shared touch, audio, camera, sensors, thermal, haptics and keys;
8. shared MDSS/display-controller data that is not panel-specific.

- [ ] **Step 2: Add only the minimal evidence-backed overrides for the group**

Use labels and symbolic references from the maintained Qualcomm source. New RED-only nodes must have descriptive labels; never preserve decompiler-generated numeric phandles. Put data identical across all four stock PVT DTBs in `common.dtsi`; put PVT-stage-only data in `pvt.dtsi`.

- [ ] **Step 3: Rebuild all four after every group**

Expected: DTC completes without new errors. Do not suppress diagnostics to make a group pass.

- [ ] **Step 4: Re-run the group checks against all four DTBs**

Expected: the added property checks pass for every variant, while the four root display IDs remain distinct.

- [ ] **Step 5: Run explicit SmartPort and essential-path checks**

Expected in each decompiled DTB:

```text
no cloudminds,smartport
no cm,smartp-req
present snps,dwc3
present qcom,usb-ssphy-qmp-v2
present qcom,qpnp-smb2
present qcom,fg-gen3
present UFS node at 0x1da4000
```

Bluetooth checks must use the exact node/compatible names measured in Task 1 rather than a donor name.

- [ ] **Step 6: Commit the shared hardware milestone**

```bash
git add arch/arm/boot/dts/qcom/msm8998-red-hydrogenone-common.dtsi \
        arch/arm/boot/dts/qcom/msm8998-red-hydrogenone-pvt.dtsi
git commit -m "arm64: dts: add Hydrogen One common hardware"
```

### Task 4: Add the four display-specific panel layers

**Files:**
- Modify: `arch/arm/boot/dts/qcom/msm8998-red-hydrogenone-panel-jdi.dtsi`
- Modify: `arch/arm/boot/dts/qcom/msm8998-red-hydrogenone-panel-sim.dtsi`
- Modify: `arch/arm/boot/dts/qcom/msm8998-red-hydrogenone-panel-tm-csp.dtsi`
- Modify: `arch/arm/boot/dts/qcom/msm8998-red-hydrogenone-panel-tm.dtsi`

**Interfaces:**
- Consumes: cross-PVT display diffs from Task 1 and common MDSS data from Task 3.
- Produces: one isolated panel/display configuration for each top-level PVT DTS.

- [ ] **Step 1: Write failing per-variant display checks**

For each built DTB, assert its stock `cm,display-id`, selected panel compatible,
panel timing/mode, reset sequence, supply references and touch/display coupling.
Expected before implementation: checks beyond the root display ID fail.

- [ ] **Step 2: Implement JDI PVT display data**

Move only JDI-specific nodes/properties from the Task 1 manifest into the JDI
panel DTSI. Rebuild and check the JDI DTB; also rebuild the other three to prove
the layer does not leak.

- [ ] **Step 3: Implement SIM PVT display data**

Repeat the same isolation/build/check cycle for display ID `0x7f`.

- [ ] **Step 4: Implement TM CSP PVT display data**

Repeat the same isolation/build/check cycle for display ID `0x0e`.

- [ ] **Step 5: Implement TM PVT display data**

Repeat the same isolation/build/check cycle for display ID `0x02`.

- [ ] **Step 6: Check cross-variant ownership**

Expected: properties identical in all four outputs are sourced from the common
or PVT layer; properties that genuinely differ are confined to panel layers.
No layer contains SmartPort nodes or GPIOs.

- [ ] **Step 7: Commit the display milestone**

```bash
git add arch/arm/boot/dts/qcom/msm8998-red-hydrogenone-panel-*.dtsi
git commit -m "arm64: dts: add Hydrogen One PVT displays"
```

### Task 5: Verify exact defconfig regeneration and DTB construction

**Files:**
- Modify if normalization requires it: `arch/arm64/configs/lineageos_hydrogenone_defconfig`
- Read: all new Hydrogen One DTS/DTSI files

**Interfaces:**
- Consumes: complete source layers from Tasks 2–4.
- Produces: a reproducible defconfig and four validated standalone DTBs.

- [ ] **Step 1: Regenerate the configuration in a fresh output directory**

Run `lineageos_hydrogenone_defconfig`, save the resolved `.config` hash, and
verify `CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE=y` plus the exact four-name list.

- [ ] **Step 2: Round-trip all four DTBs**

For each DTB, run DTB → sorted DTS → DTB → sorted DTS. Expected: the two sorted
DTS representations compare equal, apart from harmless DTC formatting.

- [ ] **Step 3: Re-run metadata, required-node and forbidden-node checks**

Expected: four passes, with no generic board model and no SmartPort marker.

- [ ] **Step 4: Check source quality**

Run `git diff --check` and inspect DTC warnings. Expected: no new whitespace
errors and no ignored new DTS warning.

- [ ] **Step 5: Commit a defconfig normalization only if it changed**

```bash
git add arch/arm64/configs/lineageos_hydrogenone_defconfig
git commit -m "arm64: configs: select Hydrogen One PVT DTBs"
```

Skip this commit if the file is already identical to Task 2.

### Task 6: Build and verify the complete kernel artifact

**Files:**
- Generate outside Git: `/tmp/h1-pvt-dts-build-final/`
- Read: `/home/surface/Downloads/H1A1000.082ho.01.00.10r.118_userdebug_fastboot/fastboot/boot.img`

**Interfaces:**
- Consumes: all source and configuration commits.
- Produces: final `Image.gz`, four DTBs, `Image.gz-dtb`, modules, hashes, DTB order proof and boot-size budget.

- [ ] **Step 1: Start from a fresh output directory and resolve defconfig**

Use these exact toolchain settings for every make invocation:

```bash
ARCH=arm64
CC=/home/surface/los/prebuilts/clang/host/linux-x86/clang-r530567/bin/clang
LD=/home/surface/los/prebuilts/clang/host/linux-x86/clang-r530567/bin/ld.lld
AR=/home/surface/los/prebuilts/clang/host/linux-x86/clang-r530567/bin/llvm-ar
NM=/home/surface/los/prebuilts/clang/host/linux-x86/clang-r530567/bin/llvm-nm
OBJCOPY=/home/surface/los/prebuilts/clang/host/linux-x86/clang-r530567/bin/llvm-objcopy
OBJDUMP=/home/surface/los/prebuilts/clang/host/linux-x86/clang-r530567/bin/llvm-objdump
STRIP=/home/surface/los/prebuilts/clang/host/linux-x86/clang-r530567/bin/llvm-strip
LLVM=1
LLVM_IAS=1
CLANG_TRIPLE=aarch64-linux-gnu-
CROSS_COMPILE=/home/surface/los/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android-
CROSS_COMPILE_ARM32=/home/surface/los/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.9/bin/arm-linux-androideabi-
HOSTCC=gcc
HOSTCXX=g++
```

- [ ] **Step 2: Run the full parallel build**

Build `Image.gz-dtb Image.gz modules` with `-j8`. Expected: exit `0`, four RED
DTBs exist, the same ten modules as the verified defconfig build exist,
and no generic Qualcomm DTB is concatenated into `Image.gz-dtb`.

- [ ] **Step 3: Prove concatenation bytes and order**

Verify byte-for-byte that:

```text
Image.gz-dtb = Image.gz
             + TM PVT DTB
             + TM CSP PVT DTB
             + SIM PVT DTB
             + JDI PVT DTB
```

Use a streaming comparison or a temporary concatenation under `/tmp`; never
add generated binaries to Git.

- [ ] **Step 4: Calculate the boot partition budget**

Calculate the conservative header-v1 projection:

```text
4096
+ align_up(sizeof(Image.gz-dtb), 4096)
+ align_up(9466573, 4096)
+ 1320
```

Expected: the result is strictly less than `67108864`. Report remaining bytes
and percentage. This is a size check, not a bootable-image claim.

- [ ] **Step 5: Record artifact hashes and warnings**

Capture SHA-256 and byte sizes for `.config`, the four DTBs, `Image.gz`,
`Image.gz-dtb`, `vmlinux`, `System.map` and `Module.symvers`. Classify every
warning as pre-existing, harmless generated noise, or a new blocker.

### Task 7: Record evidence and close the implementation branch

**Files:**
- Create: `docs/hydrogenone/pvt-dtb-build-evidence.md`
- Do not modify: `/home/surface/los/device/red/hydrogenone/**`
- Do not modify: `/home/surface/los/vendor/red/hydrogenone/**`

**Interfaces:**
- Consumes: Task 6 logs, hashes, round-trip checks, order proof and size result.
- Produces: auditable final evidence and a clean implementation branch ready for review/integration.

- [ ] **Step 1: Write the evidence document**

Record baseline/HEAD, source inputs, the four stock-to-source mappings, exact
build commands, artifact hashes/sizes, DTB order, boot-size projection,
SmartPort negative result, required subsystem results and the physical-test
limitation. Do not state that the phone boots.

- [ ] **Step 2: Run final verification from clean outputs**

Repeat the full build or a verified no-op build, all four decompilation checks,
the exact concatenation comparison, the size calculation and `git diff --check`.
Expected: every check passes with fresh captured output.

- [ ] **Step 3: Confirm scope isolation**

Compare the saved pre-work device/vendor status with the final status.
Expected: device retains exactly its pre-existing `BoardConfig.mk` modification
and backup file; vendor remains clean; neither tree gained changes from this
plan.

- [ ] **Step 4: Commit final evidence**

```bash
git add docs/hydrogenone/pvt-dtb-build-evidence.md
git commit -m "docs: record Hydrogen One PVT DTB build"
```

- [ ] **Step 5: Use verification and branch-finishing workflows**

Invoke `superpowers:verification-before-completion`, then
`superpowers:finishing-a-development-branch`. Integrate into `lineage-22.2`
only after the evidence-backed review passes. Push to the configured GitHub
repository only after confirming the exact remote URL and that no unrelated
history or files will be published.
