# Hydrogen One network spinlock watchdog diagnostics

## Original runtime failure

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

The resulting pstore stack identified the WLAN TSO transmit path:

```text
ol_tx_tso_get_stats_idx [wlan]
ol_tx_ll_fast [wlan]
ol_tx_data [wlan]
hdd_hard_start_xmit [wlan]
tcp_sendmsg
```

## Root cause and production fix

The production qcacld object is compiled with `FEATURE_TSO`, but without
`FEATURE_TSO_DEBUG`. `ol_tx_tso_get_stats_idx()` still takes
`pdev->stats.pub.tx.tso.tso_stats_lock` in that configuration, while the
non-debug implementations of `ol_txrx_tso_stats_init()` and
`ol_txrx_tso_stats_deinit()` were empty. With `CONFIG_DEBUG_SPINLOCK`, the first
TSO statistics access therefore detected a zero spinlock magic and deliberately
triggered the watchdog.

The production branch now creates and destroys `tso_stats_lock` whenever
`FEATURE_TSO` is enabled, independently of `FEATURE_TSO_DEBUG`. TSO, spinlock
validation, and the diagnostic watchdog remain enabled. The fix is commit
`4e985b5e2f22f23c3cde238b110980d270c9798d`.

`tests/test_qcacld_tso_lock_lifecycle.py` guards the non-debug lifecycle. The
compiled `ol_txrx.o` was also inspected and contains a call to
`__raw_spin_lock_init` in the real production feature combination.

## Build verification

The full `mka bacon -j7` build completed successfully and VINTF compatibility
was reported as `compatible`.

- OTA SHA-256: `8ce3b8552924b1ada15ec1e2d3c1cef2af9b1eb9d3ce1c24ed6d01cd782ae250`
- boot image SHA-256: `304ad61d57dc68bf21b94e1f251ffd45e35264148c7f83faf7dc823c4204d6f3`
- kernel SHA-256: `68f24c8e6e66e0f9e3eda98fb6b6ec92ec703ddc3c8ecff51aa056266b61fa03`
- kernel payload: 15,655,358 bytes, leaving 1,121,858 bytes below 16 MiB

## Runtime verification

The fixed 2026-09-16 OTA was installed through Lineage Recovery. Jelly loaded
Google and YouTube content over Wi-Fi for more than two minutes while full
Android and kernel logs were captured. During the test:

- the boot ID remained `e5d37474-ea87-48e5-abc9-eac967faaa97`;
- Jelly remained alive after the test;
- no Jelly or WebView fatal event was recorded;
- no `spinlock bad magic`, `ol_tx_tso_get_stats_idx`,
  `hdd_hard_start_xmit`, watchdog bite, or kernel panic was recorded.

This reproduces the workload that previously reset the entire device and
confirms the TSO spinlock fix at runtime. The captured logs are stored locally
under
`/home/surface/los/logs/hydrogenone/browser-tso-retest-20260916-023147/`.

The same capture exposed a separate `com.android.nfc` SIGSEGV restart loop. It
did not reset the device and is not part of the WLAN TSO failure.

## 5 GHz observation

The same runtime was connected on 5745 MHz using 802.11ac. The framework and
driver advertised US channels 36, 40, 44, 48, 149, 153, 157, 161, and 165, and
the scan cache contained multiple 5745 MHz results. No 5 GHz regulatory or
driver change is justified by the collected evidence. Because the 2.4 GHz and
5 GHz radios advertise the same SSID, Android may initially associate on
2412 MHz and later roam to 5745 MHz after background network selection.
