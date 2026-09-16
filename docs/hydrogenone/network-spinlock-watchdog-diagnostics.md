# Hydrogen One network spinlock watchdog diagnostics

## Runtime failure

LineageOS 22.2 boots and the RED Android 9 `.118` WLAN firmware path brings up
`wlan0`. Launching Jelly then starts the WebView network service and reproduces
a full device reset. The previous-boot ramoops log records:

```text
BUG: spinlock bad magic on CPU#2, NetworkService/4904
lock: 0xffffffd76f126178, .magic: 00000000
Causing a watchdog bite!
```

This is a kernel watchdog reset, not an application or `system_server` crash.
The zero spinlock magic indicates an uninitialised, freed, or layout-incompatible
lock, but the original image reset before recording the call stack needed to
identify its owner.

## Diagnostic change

`spin_dump()` now calls `dump_stack()` before
`msm_trigger_wdog_bite()`. Spinlock validation and the subsequent watchdog reset
remain enabled. The change only preserves the failing call chain in pstore.

`tests/test_spinlock_crash_diagnostics.py` guards that ordering. The compiled
ARM64 object was also inspected after the build and contains the calls in this
order:

```text
R_AARCH64_CALL26 dump_stack
R_AARCH64_CALL26 msm_trigger_wdog_bite
```

## Build verification

The full `mka bacon -j7` build completed successfully and VINTF compatibility
was reported as `compatible`.

- OTA SHA-256: `37ddd020acedfd645d2d3ff7acb55915ffc708ce505fada05b6d9a045a9ff251`
- boot image SHA-256: `304ad61d57dc68bf21b94e1f251ffd45e35264148c7f83faf7dc823c4204d6f3`
- kernel payload: 15,655,358 bytes, leaving 1,121,858 bytes below 16 MiB

Installation and one deliberate reproduction are still required before the
failing kernel function can be identified.

## 5 GHz observation

The same runtime was connected on 5745 MHz using 802.11ac. The framework and
driver advertised US channels 36, 40, 44, 48, 149, 153, 157, 161, and 165, and
the scan cache contained multiple 5745 MHz results. No 5 GHz regulatory or
driver change is justified by the collected evidence.
