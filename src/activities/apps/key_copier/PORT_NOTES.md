# Key Bitting Charts — wip / unverified

Key-type range metadata adapted from
`biscuit-reference/src/activities/apps/KeyCopierActivity.h`, MIT,
Copyright (c) 2025 Dave Allie. The approved ledger scope is charts only.

Displays Kwikset KW1, Schlage SC1, Yale Y1 and generic six/seven-position
reference ranges as schematic ordinal depth rows. No key matching, physical
measurement, captured credential, SD import/save or cutting profile exists.
The screen explicitly says not to scale. Manufacturer dimensional accuracy
is not asserted; source ranges are inherited and need final reference review.

One integer selection in the activity, constexpr metadata in flash, short
local text buffers, no additional owned heap. Budget object size is an estimate.

P2 validates compilation only. Hardware: Tools → Key Bitting Charts;
Up/Down or Left/Right cycles five profiles, Back exits. Check profile titles,
range rows and clipping in both orientations. Host/simulator/soak/hardware
validation remains deferred to V1. No cache reset is required.
