# ARM64 Module CRC Relocation Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore loadable external kernel modules under ARM64 KASLR so the matching `wlan.ko` can reach the ICNSS/WLFW handshake.

**Architecture:** Keep qcacld as a module to preserve the RED bootloader kernel-size margin. Restore the ARM64 module-loader relocation contract removed by `f3819ee`, rebuild kernel and module from one output tree, then repack only the known-working boot ramdisk with the corrected kernel payload.

**Tech Stack:** Linux 4.4.302, ARM64 KASLR, `CONFIG_MODVERSIONS`, Clang 19/LLD, Python `unittest`, Android boot image tools.

**Spec:** This document's Observed Failure and Global Constraints sections capture the user-confirmed ADB diagnosis.

## Observed Failure

- The installed `wlan.ko` matches all 435 required symbols in the build's `Module.symvers`.
- The module records `module_layout=0x13d71df1`.
- The running kernel reports `0xaa5171df1` because the KASLR delta `0xa91400000` is not removed.
- Runtime `_text=0xffffff8a99480000`; link-time `_text=0xffffff8008080000`.

## Global Constraints

- Modify only the isolated kernel branch `codex/diag-red118-stock-icnss-20260915`.
- Do not modify device or vendor trees.
- Keep `CONFIG_QCA_CLD_WLAN=m` and the four RED DTBs in their established order.
- Do not flash or reboot the phone during implementation.
- Repack from the currently working Lineage boot ramdisk and preserve all boot metadata.

---

### Task 1: Restore the ARM64 CRC relocation contract

**Files:**
- Modify: `tests/test_hydrogenone_defconfig_contract.py`
- Modify: `arch/arm64/include/asm/module.h`

**Interfaces:**
- Consumes: `CONFIG_RANDOMIZE_BASE`, `CONFIG_MODVERSIONS`, `kimage_vaddr`, and `KIMAGE_VADDR`.
- Produces: `ARCH_RELOCATES_KCRCTAB` and `reloc_start` for `kernel/module.c::maybe_relocated()`.

- [x] **Step 1: Write the failing preprocessor contract test**

  Compile `#include <asm/module.h>` with both relevant config macros enabled and assert that preprocessing defines `ARCH_RELOCATES_KCRCTAB` plus `reloc_start` as `(kimage_vaddr - KIMAGE_VADDR)`.

- [x] **Step 2: Run the focused test and verify RED**

  Run `python3 -m unittest tests.test_hydrogenone_defconfig_contract.HydrogenOneDefconfigContractTest.test_arm64_unapplies_kaslr_from_kernel_symbol_crcs -v` and require failure because `ARCH_RELOCATES_KCRCTAB` is absent.

- [x] **Step 3: Implement the minimal fix**

  Include `asm/memory.h`; under `CONFIG_RANDOMIZE_BASE && CONFIG_MODVERSIONS`, define `ARCH_RELOCATES_KCRCTAB` and `reloc_start (kimage_vaddr - KIMAGE_VADDR)`.

- [x] **Step 4: Verify GREEN and the full contract suite**

  Run the focused test, then all kernel Python contract tests. Require all tests to pass.

- [x] **Step 5: Commit the isolated fix**

  Commit only the test, ARM64 header, and this plan with message `arm64: restore KASLR module CRC relocation`.

### Task 2: Build and verify a corrected diagnostic boot

**Files:**
- Build output: `/home/surface/los/out/codex/kernel-red118-stock-icnss-crcfix`
- Artifact: `/home/surface/los/out/codex/red118-stock-wlfw-crcfix/boot-red118-stock-wlfw-crcfix-4.4.302.img`

**Interfaces:**
- Consumes: the committed kernel source, `lineageos_hydrogenone_defconfig`, and the known-working boot ramdisk.
- Produces: one boot image with a corrected kernel plus a matching stripped `wlan.ko` for verification.

- [ ] **Step 1: Perform one clean kernel and module build**

  Build `Image.gz-dtb`, `vmlinux`, and modules in one output directory using Clang `r536225` and the Android GCC 4.9 cross tools.

- [ ] **Step 2: Verify the exact module ABI**

  Compare every CRC required by the built and currently installed `wlan.ko` against the new `Module.symvers`; require zero missing and zero mismatched symbols.

- [ ] **Step 3: Verify RED runtime contracts**

  Require the four RED DTBs in TM, TM CSP, SIM, JDI order, SmartPort absence, required built-in runtime drivers, and a kernel payload below 16 MiB.

- [ ] **Step 4: Repack and inspect the boot image**

  Replace only the kernel in the known-working boot image; require byte-identical ramdisk and identical boot metadata, then record SHA-256 and partition-size margin.

- [ ] **Step 5: Stop before flashing**

  Report the verified artifact and request the user's separate confirmation before writing any partition.
