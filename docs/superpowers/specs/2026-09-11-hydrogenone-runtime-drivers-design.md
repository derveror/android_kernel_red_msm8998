# Hydrogen One runtime drivers design

Date: 2026-09-11
Status: design approved in chat

## Goal

Add the source-built kernel drivers required by the four RED Hydrogen One PVT
device trees on the maintained Linux 4.4.302 base. The work covers FPC1020,
LM36923H, TFA9894 and Cypress CYTTSP5 in separate, reviewable changes. It does
not change the device or vendor repositories and does not restore the
proprietary rear SmartPort.

This is a source and static-verification phase. A successful compile is not a
claim that display, touch, fingerprint or speakers work on physical hardware.

## Authority and starting point

The implementation starts from kernel commit
`20185393a9d460125493ae421b34d9616bd41628` on branch `lineage-22.2` in
`derveror/android_kernel_red_msm8998`.

Hardware values and runtime contracts are taken in this order:

1. the supplied RED `.118` Android 9 `boot.img`, its extracted Linux
   `4.4.153+` Image, config and production DTBs;
2. the supplied `.118` `vendor.img` and current RED userspace contracts;
3. licensed public implementations for the same hardware family;
4. maintained MSM8998 donor implementations as API and 4.4.302 integration
   references.

Donor GPIOs, regulators, firmware names, panel data and board identity must
never replace stock RED values.

## Confirmed starting facts

| Component | Stock `.118` evidence | Current 4.4.302 state | Licensed source position |
|---|---|---|---|
| FPC1020 | built-in `CONFIG_INPUT_FPC_FINGERPRINT=y`; source-path string `drivers/input/misc/fpc1020_platform_tee.c`; `fpc,fpc1020`; stock sysfs and PM symbols | exact RED DTS wiring exists; no matching driver/config | Qualcomm/AOSP and multiple MSM8998 donors contain GPL-2.0 platform/TEE resource drivers |
| LM36923H | built-in `CONFIG_BACKLIGHT_LM36923H=y`; source-path string `drivers/video/backlight/lm36923h_bl.c`; LED class `LM36923H-BL` | exact RED DTS node exists; no driver/config | upstream TI `leds-lm3692x` supports the chip family; no supplied donor contains the RED Android wrapper |
| TFA9894 | built-in `CONFIG_SND_SOC_TFA9894=y`; source-path strings under `sound/soc/codecs/tfa9894`; revision `0x94`; `nxp,tfa98xx` | exact two-amplifier DTS and MI2S selection exist; no TFA98xx driver/config | Nokia MSM8998 `lineage-22.2` has a GPL driver with explicit TFA9894 revision and operations |
| Cypress CYTTSP5 | JDI DTB contains the complete I2C/core/MT/button/proximity topology; `.118` config explicitly disables `CONFIG_USE_TOUCHSCREEN_CYPRESS_CYTTSP5` | JDI DTS exists; only older CYTTSP4 code is present | GPL Cypress Gen5 code with the same `cy,cyttsp5_i2c_adapter` binding exists in AOSP kernel sources |
| Synaptics DSX | stock TM/TM-CSP touch binding | driver and binding support already compile in the target tree | no new driver family required |

The `.118` vendor image contains:

- `/vendor/etc/firmware/tfa98xx.cnt`, 24,229 bytes, SHA-256
  `6cbe57596dbd02fd093ffac649c8c069d6d9c1594c5c3238d734a1ccb190fa2d`;
- `/vendor/etc/firmware/tfa98xx_a3d.cnt`, 21,676 bytes, SHA-256
  `f16a32b53fead188d7ed30827448160c59dc75af2ea763c79a96fc9089178908`.

Those files are absent from the current vendor repository. This fact is
recorded here, but copying them or modifying vendor packaging is outside this
kernel-only phase.

## Chosen architecture

Port one driver family at a time. Each family receives its own Kconfig and
Makefile integration, exact stock-facing config symbol, binding documentation,
contract test, full kernel build and commit. The order is:

1. FPC1020 fingerprint resource driver;
2. LM36923H backlight driver;
3. TFA9894 smart-amplifier driver;
4. Cypress CYTTSP5 for the JDI variant;
5. combined four-DTB and kernel regression verification.

This order starts with the highest-confidence source port, isolates the
backlight work whose RED dual-chip extension needs extra proof, keeps audio
separate from its missing vendor firmware packaging, and handles the
stock-disabled JDI touch family last.

Bulk-copying all donor driver directories is rejected because it would import
unrelated device policy and obscure the first failing subsystem. Leaving the
nodes unbound or retaining stock `.ko` files is also rejected: the relevant
drivers are built into the stock kernel and a stock 4.4.153 module would not
have the 4.4.302 source kernel's verified ABI.

## Cross-cutting implementation rules

- Work only on `lineage-22.2`; do not force-push or rewrite published history.
- Record source repository, branch/ref, commit SHA, file paths, license and
  reason for every imported file in `docs/references/COMMIT_PROVENANCE.csv` or
  the nearest existing provenance record created by the implementation plan.
- Preserve the four DTB names and order: TM, TM CSP, SIM, JDI.
- Preserve all RED GPIOs, regulators, I2C addresses and firmware filenames from
  the compiled stock-derived DTS layer.
- Build the four drivers into the kernel (`=y`) unless a source dependency
  proves that a module is required. No stock binary kernel module is accepted
  as a substitute.
- Do not add SmartPort nodes, source, Kconfig symbols, serial hooks or stubs.
- Do not modify `/home/surface/los/device/red/hydrogenone` or
  `/home/surface/los/vendor/red/hydrogenone` during this phase.
- Do not weaken SELinux, module signature/version checks, AVB, VINTF or Android
  security requirements.
- Never invent unobserved RED behavior. An uncertain hardware sequence is a
  documented runtime blocker, not a reason to copy a donor's value.

## FPC1020 design

### Source and layout

Use the Razer MSM8998 `lineage-22.2` FPC platform driver at commit
`1ea8ada363c839064efd290f81ed131bcae85289` as the Linux 4.4.302 integration
base and compare it function-by-function with the Qualcomm/AOSP
regulator-aware implementation. Install the selected and minimally adapted
code under `drivers/input/fingerprint/`, connected through
`drivers/input/Kconfig` and `drivers/input/Makefile`.

Expose `CONFIG_INPUT_FINGERPRINT` and the stock-facing
`CONFIG_INPUT_FPC_FINGERPRINT`; enable both in
`lineageos_hydrogenone_defconfig`.

### Required RED contract

The driver must bind only to `fpc,fpc1020` and consume the existing RED DTS
properties:

- `fpc,gpio_irq` on TLMM 121;
- `fpc,gpio_rst` on TLMM 32;
- the three named FPC pinctrl states;
- `vcc_spi`, `vdd_ana` and `vdd_io` supplies;
- `fpc,enable-on-boot`.

It is a resource/interrupt bridge for the proprietary TEE userspace stack; it
must not implement fingerprint image capture or matching in the normal world.

The sysfs group must provide the stock-observed resource controls and at least
the three paths consumed by current RED init: `irq`, `wakeup_enable` and
`wakeup_feature`. The public base semantics may be reused for `irq`, power,
reset and `wakeup_enable`. The RED-only `wakeup_feature` handler may be added
only after static analysis establishes its stock write values and effect. If
that effect cannot be established unambiguously, the driver port can compile
but FPC runtime status remains BLOCKED rather than receiving a guessed handler.

Probe must unwind regulators, wakeup source, IRQ, sysfs group and pinctrl state
on every failure path. Remove and PM paths must not double-disable IRQs or leave
supplies enabled.

## LM36923H design

### Source and layout

Use the GPL upstream TI LM3692x implementation and the LM36923H Rev. B data
sheet for register definitions. Put the RED-compatible Android wrapper at the
stock-family location `drivers/video/backlight/lm36923h_bl.c`, controlled by
`CONFIG_BACKLIGHT_LM36923H`, and enable it built-in.

### Required RED contract

The driver must bind to `ti,lm36923h`, consume `hwen-gpio`, honor
`current-limitation`, and expose the stock LED-class name `LM36923H-BL` so the
existing brightness path remains valid. It must use the device's 11-bit
brightness registers and preserve zero-brightness disable behavior.

`dual-3d-bkl` selects RED's second-backlight path. The ordinary single-chip
backlight can use documented TI register behavior. The second-chip address and
write sequence must be obtained from unambiguous stock-binary evidence or a
licensed matching source before the dual path is enabled. If that evidence is
not recovered, normal backlight can reach static completion while the Leia
dual-backlight path remains explicitly BLOCKED and unclaimed.

Debug register write interfaces from the stock binary are not required for
production. Only read-only diagnostics may be retained when useful for bring-up.

## TFA9894 design

Use the Nokia MSM8998 `lineage-22.2` TFA98xx family at commit
`d18a7df9af20cb6e7d02621ea85a23b83d0e7d8c` as the primary 4.4 source,
trimmed to the files and chip operations required for revision `0x94`. Expose
the stock-facing `CONFIG_SND_SOC_TFA9894`, enable it built-in, and bind to both
`nxp,tfa98xx` and `nxp,tfa9894`.

The driver must consume the RED `reset-gpio`, `irq-gpio` and
`tfa98xx,fw-name` properties, support both I2C devices at `0x34` and `0x35`,
register address-qualified DAIs compatible with the existing Qualcomm MI2S
machine driver, and request the exact `.cnt` filenames without embedding the
proprietary containers in the kernel repository.

Static completion requires compilation, probe-path inspection and exact
firmware request strings. Runtime speaker completion additionally requires the
two measured stock containers to be packaged by the later vendor-integration
phase and tested on hardware.

## Cypress CYTTSP5 design

Use a GPL AOSP Cypress Gen5 source that implements the exact
`cy,cyttsp5_i2c_adapter` devicetree model. Import only the core, devicetree,
I2C, MT-B, button and proximity units required by the JDI subtree. Firmware
loader and device-access/debug modules are excluded unless the stock JDI
contract proves they are required.

Enable the driver built-in only after it compiles against Linux 4.4.302 and the
flattened JDI DTB passes a binding-consumer test. Because the supplied `.118`
config disables the Cypress Gen5 family, static source support does not prove
that the provided stock release ever exercised JDI touch. Runtime status stays
unverified until tested on an actual JDI unit.

## Tests and evidence

Before each driver is added, a contract check must fail because its compatible,
config symbol or required runtime interface has no source consumer. After the
minimal port, the same check must pass.

Every driver commit must pass:

1. `git diff --check` and targeted `checkpatch.pl` on new/ported files;
2. `lineageos_hydrogenone_defconfig` regeneration and exact symbol check;
3. clean `Image.gz-dtb Image.gz modules` build with the established Clang 19
   toolchain and checked exit code;
4. all four standalone DTB builds and DTB -> sorted DTS -> DTB -> sorted DTS
   round trips;
5. exact `Image.gz-dtb = Image.gz + TM + TM CSP + SIM + JDI` byte comparison;
6. no SmartPort markers in any compiled DTB or newly added source;
7. no changes in device/vendor repository status beyond the device tree's
   already existing local `BoardConfig.mk` state;
8. a build-evidence update containing command, log path, source SHAs, artifact
   hashes, warnings and explicit runtime limitations.

Driver-specific static checks must also confirm:

- FPC: compatible, config, RED GPIO/supply property names and required sysfs
  attribute names;
- LM36923H: compatible, LED class name, 11-bit brightness registers and
  property consumers;
- TFA9894: revision `0x94`, two RED I2C devices, DAI registration and exact
  firmware names;
- Cypress: JDI-only compatible and required MT/button/proximity child consumers.

No commit is pushed until its clean build and static gates pass. After a push,
the remote `lineage-22.2` SHA must be compared with the local SHA.

## Completion and stop conditions

This phase is complete when all source families that have sufficient evidence
compile in one clean 4.4.302 build, the four DTBs and appended order remain
unchanged, provenance is recorded, and every unresolved runtime requirement is
named explicitly.

Stop the relevant substage rather than guessing if:

- a source license or commit identity is unclear;
- a donor API requires unrelated board-specific code;
- the stock and public bindings require contradictory electrical values;
- RED dual-backlight behavior cannot be proven;
- TFA compilation requires proprietary source rather than the `.cnt` firmware;
- Cypress code cannot consume the exact JDI subtree without hardware-specific
  data absent from stock evidence.

Physical boot and hardware validation remain separate later stages. A clean
kernel build does not change any hardware item from UNVERIFIED to PASS.
