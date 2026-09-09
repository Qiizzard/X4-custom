# X3/X4 test binary — 2026-09-09 UTC

Delivered locally as `updated.bin` from the C3 default build verified in batch 7.
Source integration: 99ca9764, 3c70ae7a, 8994be8e; evidence: 349d3d26.
Later delivery edits update documentation only. Shipping partitions unchanged.

- Size: 6,316,352 bytes; OTA slot: 6,553,600 bytes.
- SHA-256: `4099500e9c075e11212efa1c4e4456ea6d03382cd173f56fd4eba9ae2643ed37`.
- esptool image-info identifies ESP32-C3 and reports checksum and validation
  hash valid. Host tests, UBSan, simulator and C3 build results are in batch 7.
- For X3/X4, not X4 Pro or Sticky. New apps remain wip for their hardware gates.

Copy updated.bin onto the SD card and select it in Settings → SD Card Firmware
Update. The existing picker accepts .bin files; the filename need not be
update.bin. No device was flashed in preparing this artifact. Generated binary
and checksum were delivered separately, not committed to source control.
