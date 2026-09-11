# Hydrogen One PVT Device-Tree Design

Date: 2026-09-11

## Context

The target is the `lineage-22.2` branch of
`kernel/red/msm8998`, based on Linux 4.4.302. The existing
`lineageos_hydrogenone_defconfig` builds `Image.gz` and modules with Clang 19,
but the tree does not yet contain RED Hydrogen One device trees and therefore
cannot produce a device-specific appended-DTB kernel image.

The stock `.118` boot image contains 57 appended DTBs. Twenty-two are RED
Hydrogen One variants; the rest are generic Qualcomm development boards. The
first source bring-up will support the four production/PVT RED variants, while
development EVT/DVT variants remain outside the initial scope.

## Goals

- Add maintainable source DTS/DTSI files for all four production/PVT display
  variants.
- Preserve the stock bootloader-selection identifiers and stock PVT ordering.
- Keep ordinary USB-C, Bluetooth, handset charging, fuel-gauge, storage,
  display and other boot-critical hardware paths in scope.
- Build only the four RED PVT DTBs into the appended-DTB kernel image selected
  by `lineageos_hydrogenone_defconfig`.
- Keep every RED-specific hardware value traceable to the supplied stock boot
  image or RED device/vendor material.

## Non-goals

- Supporting the proprietary rear CloudMinds/RED SmartPort, its GPIO power
  controls, accessory UART request line or accessory PCIe enumeration.
- Claiming support for EVT, EVT2.1, EVT2.1C or DVT hardware.
- Reusing panel, GPIO, charging, camera, touch or calibration values from a
  different MSM8998 device.
- Claiming runtime or boot success without a physical flash-and-boot test.

## Selected variants and order

The appended order will match the relative order of the four PVT entries in
the supplied stock image:

| Order | Stock entry | Variant | `fih,hw-id` | `cm,display-id` |
|---:|---|---|---|---|
| 1 | `stock.dtb.1` | JDI PVT | `<4 4 0>` | `<0x64>` |
| 2 | `stock.dtb.4` | SIM PVT | `<4 4 0>` | `<0x7f>` |
| 3 | `stock.dtb.10` | TM CSP PVT | `<4 4 0>` | `<0x0e>` |
| 4 | `stock.dtb.19` | TM PVT | `<4 4 0>` | `<0x02>` |

All four retain the stock root matching data:

- `compatible = "qcom,msm8998-mtp", "qcom,msm8998", "qcom,mtp"`;
- `qcom,msm-id = <0x124 0x20001>`;
- `qcom,board-id = <8 0 1 0>`.

The stock-specific ordering is preserved because all four PVT images share the
same Qualcomm board and FIH hardware IDs; `cm,display-id` is their measured
RED-specific differentiator. Runtime selection behavior remains unclaimed
until a physical boot log is available.

## Source architecture

The implementation will use the existing Qualcomm 4.4 include hierarchy, not
four monolithic DTS files generated from binary DTBs:

- `msm8998-v2.1.dtsi`, `msm8998-mdss-panels.dtsi` and
  `msm8998-mtp.dtsi` provide the maintained MSM8998 v2.1 platform skeleton.
- `msm8998-red-hydrogenone-common.dtsi` contains RED hardware shared by all
  production variants.
- `msm8998-red-hydrogenone-pvt.dtsi` contains the common PVT-stage overrides.
- Small panel/display-specific DTSI files contain only the differences proven
  for JDI, SIM, TM CSP and TM.
- Four top-level DTS files define the model and exact selection identifiers and
  include the shared layers.

The common layer owns ordinary USB-C/DWC3 and PHY overrides, Bluetooth power
and WCN3990 integration, QPNP SMB2/SMB138x charging, FG Gen3, UFS, regulators,
thermal data, keys, audio and other board-wide production data. Panel layers
must not duplicate unrelated board configuration.

## Evidence and provenance rules

The supplied stock PVT DTBs are the authority for RED board values. Decompiled
content may be used to measure nodes, properties and relationships, but a
full decompiler dump with generated numeric phandles will not be checked in.
References from other MSM8998 devices may be used only for source organization,
binding syntax and build mechanics. Their device-specific electrical values
must not enter the RED files.

Each implemented subsystem will be compared across all four PVT stock DTBs:
identical data belongs in the common layer; genuine display-dependent changes
belong in a variant layer. An unresolved difference is omitted or disabled and
documented rather than guessed.

## SmartPort boundary

No `cloudminds,smartport` node, `cm,smartp-req` pinmux selection or external
module power GPIO control will be added. This does not remove or disable:

- DWC3, QUSB/QMP PHY, USB gadget/configfs or normal USB-C charging;
- the handset Bluetooth controller and its power/slimbus integration;
- QPNP SMB2, SMB138x, USB-PD or fuel-gauge support;
- the internal Leia/display panel paths.

## Build integration

The Qualcomm DT Makefile will list the four new DTBs. The Hydrogen One defconfig
will set `CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES` to the four
`qcom/...` names in the stock PVT order. This prevents the fallback behavior
that appends every generic Qualcomm DTB found in the output tree.

The primary deliverable is `arch/arm64/boot/Image.gz-dtb`; `Image.gz` and
modules continue to be built as independent verification artifacts. Device-side
packaging changes are deferred until the kernel artifact and its DTB order are
verified.

## Verification

Before accepting the implementation:

1. Regenerate `.config` from `lineageos_hydrogenone_defconfig` and confirm the
   exact four-name appended-DTB list.
2. Build each DTB, `Image.gz-dtb`, `Image.gz` and modules with the established
   Clang 19 toolchain.
3. Decompile the four built DTBs and verify their model, compatible strings,
   Qualcomm IDs, FIH PVT ID and display IDs.
4. Verify the concatenated image contains exactly four DTBs in the selected
   JDI, SIM, TM CSP, TM order.
5. Verify that no built PVT DTB contains `cloudminds,smartport` or
   `cm,smartp-req`.
6. Verify that all four retain the required ordinary USB, Bluetooth, charging,
   fuel-gauge, UFS and display nodes expected from their stock counterparts.
7. Run repository whitespace checks and record the build commands, hashes and
   static limitations.

A successful static build proves source consistency and packaging shape only.
Boot, display output, touch, radio, charging and other hardware results remain
unverified until the intentionally skipped physical stage is performed.

## Failure handling

- A DTS compiler error is fixed at the smallest owning layer; it is not hidden
  by suppressing diagnostics.
- If an essential stock relationship cannot be expressed against the 4.4.302
  include tree without guessing, implementation stops at that subsystem and
  records the exact missing evidence.
- If a panel-specific property differs among PVT variants, it is split out even
  when duplication would be shorter.
- No build result is committed as complete unless all four selected DTBs and
  the full kernel/modules build pass.

## Completion criteria

This stage is complete when the four layered PVT device trees are present,
their exact appended order is configured, the full static verification above
passes, and the results are committed to `lineage-22.2`. Physical boot remains
a separate, explicitly unverified milestone.
