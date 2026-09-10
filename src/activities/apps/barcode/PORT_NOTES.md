# Barcode — wip / unverified

Adapted from `biscuit-reference/src/activities/apps/BarcodeActivity.{h,cpp}`,
MIT, Copyright (c) 2025 Dave Allie. Supports Code 128B, Code 39 and EAN-13.

Retains a 49-byte fixed payload. Shared keyboard limits are 48/32/13 bytes;
its fallible allocation is checked and its result validated before copying.
The keyboard owns temporary strings on this cold input path; there is no
per-frame string allocation. Unsupported characters are rejected, Code 39
letters normalized to uppercase, and oversized barcodes rejected instead of
clipped. Existing EAN digit/checksum validation is retained. Bounded payloads
replace two label-copy loops that could overrun their final NUL byte.

Corrected numeric Code 128 widths at indices 36–41 and 92–102 against the
[ZXing Code128Reader reference table](https://raw.githubusercontent.com/zxing/zxing/master/core/src/main/java/com/google/zxing/oned/Code128Reader.java),
accessed 2026-09-10. These are symbol-data corrections in the Biscuit table;
no ZXing decoder implementation or runtime dependency is included.
This source comparison is not a scanner or full encoding test pass.

C3 compile sanity only in P2; host/decoder/simulator/soak/hardware validation
is deferred to V1. Memory manifest figures remain estimates, including the
shared keyboard transient. Check all ASCII symbols and checksum values during
V1, plus Code 39 full alphabet and EAN parity/checksum fixtures.

Hardware: Tools → Barcode Generator. Select each type, enter data, cancel,
return and generate again. Scan displayed codes and compare exact decoded
payloads, especially K–P and punctuation and inputs near the width limit.
Verify invalid input, invalid EAN check digit and over-wide output display an
error; check both orientations and repeated keyboard entry/exit heap. No EPUB
cache reset is required. Touch-specific interaction is deferred.
