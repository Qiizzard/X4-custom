# WiFi QR Share — wip / unverified

Reference: Biscuit WifiCredsActivity (MIT). Its manually entered SSID/password
and auth picker are adapted to the ledger's authenticated stored-secret gate:
Tools > Password Manager creates/edits records; username is SSID (1..32 bytes),
password is the Wi-Fi key. Tools > WiFi QR Share unlocks the existing real
password vault, lists records, and exposes a QR only after explicit Confirm.
No creation, editing, deletion, decoy routing, network scan or credential-store
access in this mode. Existing .next/recovery checks and crypto authentication
remain mandatory. Hardware crypto validation is still an open release gate.

Up/Down selects records/returns to list. Left/Right selects WPA/WPA2, legacy WEP,
or open before reveal; changing type hides/wipes output and requires Confirm
again. Open omits password data (the vault record may retain it); no radio/AP
is created. SSID escapes backslash, semicolon, comma, quote and colon, as does
the protected-mode password. Hidden-network flags/enterprise/WPA3-specific
formats are not claimed. Invalid field lengths fail closed; credentials are
encoded, not validated against a network. No password rendered as plain text.

At most 208 payload bytes plus NUL reuse the 1,220-byte vault scratch buffer;
v9-L matrix is 352 bytes reusing the 1,160-byte encoded buffer. No new persistent
array or framebuffer, one unsigned auth selector added. Scratch is wiped after
encoding; matrix wiped on hide, type change, list return, timeout, lock or exit.
Existing ten-second reveal and 60-second idle lock apply. qrcode.c v9 automatic
codeword/function/error-correction buffers total roughly 1 KiB at nested peak,
plus frames; large stack buffers are in the existing library, bounded by the
fixed QR version. Stack high-water and phone decode remain to measure. Library
stack remnants and photographed/persisting e-ink pixels are not erased secrets;
no resistance to physical memory inspection, screenshots or cameras is claimed.
Exit clears shared RAM framebuffer and next screen redraws; panel ghosting and
sleep-display behavior must be verified before trusting timed visual hiding.

V1/hardware: test-only vault, correct/incorrect key, tampered/recovery files,
empty vault, all8 records; username lengths 0/1/32/33/47; escaped punctuation,
Unicode byte limits, 63-byte keys; WPA/WEP/open phone decoding and successful
join to a test AP; no key in open payload; Confirm hide/10s expiry/60s lock,
auth change, Back/Home/sleep and cancellation; heap/stack and orientations.
Confirm no add/edit/delete/write in this mode and password/TOTP/duress flows
retain behavior. No cache reset, production secrets or flashing for these tests.
