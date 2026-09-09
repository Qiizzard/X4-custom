# EPUB ENGINE COMPARISON — x4-merge vs microreader vs CrossPoint/FreeInkBook

This is archived analysis inherited from an earlier session. Its external-project
comparisons, maturity judgements and performance claims below have **not been
revalidated** and are not current release evidence. Retained for provenance.
Current local evidence is in verification/BATCH_5_CSS_2026-09-08.md: CSS cache
version 17 and cold/cached selector tests pass; physical rendering comparisons
remain open. PORT_LEDGER controls scope.

## Verdict up front

`lib/Epub` is more mature than a first read of "it's a fork's fork" would
suggest. It already has several things microreader and stock CrossPoint
don't:

- Arena/bump-packed word storage in `TextBlock` (one heap arena per block
  instead of per-word vectors/strings) — the same anti-fragmentation
  philosophy as FreeInkBook's "arenas only" rule, already in production here.
- An adaptive long-text-run flush (`ChapterHtmlSlimParser::flushLongTextRunIfNeeded`,
  `parsers/ChapterHtmlSlimParser.cpp:36-42,610-650`) that is a **strictly
  better** fix than CrossPoint's own unmerged `97ee05a4` "handle crashes on
  very large EPUB chapters" branch (see below) — word- *and* byte-bounded,
  with separate tuned thresholds for CSS/bionic/guide-reading modes, vs. the
  upstream fix's flat 300-word cutoff.
- Real bidi: per-word RTL detection, paragraph-level direction inference, and
  visual line reordering (`BidiUtils::computeVisualWordOrder`, used from
  `ParsedText.cpp`) — this is further than microreader's README claims
  (hyphenation-language list only, no bidi mentioned) and roughly on par with
  FreeInkBook's UAX#9 claim.
- A hyphenation pipeline that layers explicit-hyphen splitting, apostrophe-
  contraction awareness, and Liang pattern lookup with per-language minima
  (`hyphenation/Hyphenator.h`, `LiangHyphenation.h`) — more capable than a
  bare Liang implementation.
- Streaming image decode via JPEGDEC/PNGdec callbacks straight to the
  framebuffer (`converters/JpegToFramebufferConverter.cpp`,
  `PngToFramebufferConverter.cpp`) — no full-image buffering, comparable to
  microreader's and FreeInkBook's streaming decode.

So "beat microreader and CrossPoint at EPUB handling" is not a from-scratch
problem. It's two real, addressable gaps plus one dormant asset already
sitting in our own dependency tree.

## Gap 1 (fixed this session): CSS cascade doesn't support compound classes

`lib/Epub/Epub/css/CssParser.h` documented its own cascade as fixed 4-tier
(`element < descendant < class < element.class`) with **no real specificity
computation** — no numeric specificity, no `!important`-aware tie-breaking
beyond a flat strip, no combinator support. That's already a known,
deliberate scope limit (media queries, pseudo-classes, and 3+-part
descendant selectors are explicitly out, per the class doc comment) and
isn't being relitigated here.

But `CssParser.cpp` carried its own **self-acknowledged** bug, not a scope
limit: two `// TODO: Support combinations of classes (e.g. style on
.class1.class2)` comments sat directly in `resolveStyle()` (pre-fix
`CssParser.cpp:854,878`). Tracing it: a stylesheet rule like
`.dropcap.first-letter { ... }` **was accepted and stored** by
`processRuleBlockWithStyle()` (no unsupported characters, no whitespace →
falls through to the generic "store under literal selector text" path) —
but `resolveStyle()` never constructed a matching lookup key for it. It only
ever probes `tag`, `.singleClass` (per class token), and `tag.singleClass`
(per class token). A two-class compound rule was silently dead: parsed,
held in memory, and never applied. Worse, `tag.a.b` selectors were actively
*mis-parsed* by `selectorMatchesElement()`, whose `dotPos = selector.find('.')`
takes the *first* dot only, so `selectorClass` for `"p.a.b"` became the
literal string `"a.b"` — which can never equal a real class token.

This is a real-world rendering-fidelity bug, not a theoretical one: compound
classes are how EPUB stylesheets commonly express variants (drop caps,
poetry stanza styles, pull-quote widths) without a combinatorial explosion
of single-purpose classes, and any EPUB using that pattern silently lost
that styling on this reader while microreader's real `CssStylesheet`
(`specificity()`-based, per its `CssParser.h`) would have honored it.

**Fix applied** (`lib/Epub/Epub/css/CssParser.cpp`, `CssParser.h`):
three new helpers — `splitTwoClassSelector`, `classTokenLess`,
`buildTwoClassKey` — decompose a two-class selector and build a canonical
key (classes ordered case-insensitively) so parse-time storage and
resolve-time lookup always agree on the key regardless of whether the CSS
source or the HTML `class=""` attribute lists the classes in a different
order. `processRuleBlockWithStyle()` now normalizes and stores compound
rules under that canonical key instead of dead literal text; `resolveStyle()`
gained cascade step 4 (highest priority, up to 8 class tokens on an
element, all `C(n,2)` pairs probed both bare and tag-qualified). Selectors
with three or more compounded classes remain unsupported, same as before —
scope was kept to the exact case the code's own TODOs called out, to keep
the change small and reviewable. Both `CssParser.cpp` and `CssParser.h`
doc comments were updated to describe the new behavior instead of a stale
TODO.

**Verification status (updated — a real build now exists):** the new logic
(order-independence, tag/class splitting, 3+-class rejection, overflow
safety, and a full parse→store→resolve round trip with reversed source
order) was first verified with a standalone host build extracted
byte-for-byte from the real functions (`g++ -std=c++20 -Wall -Wextra
-pedantic`, 18/18 checks pass, zero warnings), and `clang-format --dry-run
--Werror` clean. A later session got `-e simulator` building in the same
sandbox (see `docs/merge/PORT_LEDGER.md`'s "Next session starts here" for
how), and that changed what could actually be checked:

- **`CssParser.cpp`/`.h` now compile and link as part of the real firmware
  translation unit** — no longer just a standalone duplicate.
- **The general simulator smoke test's real EPUB reader pipeline exercises
  this exact compiled code** (loading, CSS-caching, rendering, and paginating
  a real `.epub`) without crashing.
- **A fixture built specifically to hit this code path** —
  `test/epubs/test_css_compound_selectors.epub`, generated by
  `scripts/generate_css_compound_test_epub.py` — declares a bare `.a.b` rule
  and a tag-qualified `p.warn.big` rule, each in one class order, and applies
  them in HTML markup in the *opposite* order (plus a same-classes-on-a-`div`
  negative control that must NOT match the tag-qualified rule, and a
  three-class selector that's documented-unsupported and must not disturb
  parsing of the rules around it). Running it through the compiled binary
  shows the CSS cache loading successfully (`[DBG] [CSS] Loaded 4 indexed
  rules + 0 descendant rules from complete cache`) and the reader rendering
  the page without error.

**What is still NOT done**: none of the above proves the bold/italic styling
actually lands on the correct runs of text at the pixel level — there is no
screenshot or framebuffer-dump capability in the simulator harness to check
that directly. The originally-suggested next step, a proper host `CTest`
target for `CssParser.cpp` itself (following the
`test/minibidi_arabic/stubs/Logging.h`-style no-op-stub pattern), turned out
to need real stubbing effort: `CssParser.h` pulls in `HalStorage.h`, whose
`HalFile : public Print` depends on `<Print.h>` and
`<freertos/semphr.h>`, and `loadFromStream()`'s only entry point takes an
`FsFile&` (no `loadFromString()` exists) — building a minimal fake `HalFile`
was judged too large to fit alongside everything else asked of the session
that found this. Still `wip`, not `done`, but the gap between the two is now
"proven correct at the pixel level" rather than "compiled at all" — a
meaningfully smaller gap than before. A future session should either build
that `HalFile` stub (the cleanest fix — it makes `CssParser.cpp` truly
unit-testable, not just crash-tested) or add a screenshot/pixel-dump
capability to the simulator harness and compare a rendered page against a
known-good reference.

## Gap 2 (documented, not yet fixed): fonts are baked bitmaps only

`lib/EpdFont` bakes `.ttf` sources (`lib/EpdFont/builtinFonts/source/**`)
into `EpdFontData` at build time via `scripts/fontconvert.py`. There is no
runtime TTF/OTF rendering path anywhere in `src/` or `lib/Epub` — confirmed
by an empty `grep -rl "stb_truetype\|FreeType"` across the firmware tree.
That means an EPUB shipping its own embedded custom font (a common
publisher practice for display/heading faces) is silently ignored; the
reader always falls back to the baked font family.

FreeInkBook (see Gap 3) already solves this with a `RenderFont` interface,
`TtfFont` via `stb_truetype`, and an up-to-8-face `FontChain`
(`freeink-sdk/docs/freeink-book.md`, Fonts section) — runtime rendering of
whatever font the EPUB actually ships. microreader's README doesn't claim
embedded-font support either, so this isn't a "we're behind microreader"
gap specifically, but it is a real "we're behind what's achievable, and a
solved reference implementation is sitting in our own submodule" gap.

This was **not attempted this session** — it's a much larger change
(runtime font loading, memory budget for glyph caches on ESP32-C3 vs S3,
`EpdFontFamily` integration) that deserves its own port-checklist-style pass
rather than being squeezed in alongside the CSS fix. Documented here as the
#2 priority for whoever picks this up next.

## Gap 3 (opportunity, not a bug): FreeInkBook sits unused in our own tree

`freeink-sdk` is vendored as a git submodule (`.gitmodules`: `Free-Ink/freeink-sdk`,
branch `main`) and contains `FreeInkBook`
(`freeink-sdk/libs/book/FreeInkBook`, 28,743 lines across headers+sources) —
a complete, independently-host-tested, actively-maintained alternative EPUB
engine. `grep -rl "FreeInkBook" src/` returns **nothing**: the actual reader
code (`src/activities/reader/EpubReaderActivity.cpp` and friends) never
references it. It appears to be CrossPoint-ecosystem's own next-generation
replacement effort — referenced from their `feat-epub-parser-refactor`
branch's "Add FreeInkBook EPUB engine migration guide" commit
(`0848649c`), which is itself unmerged into CrossPoint's own `master`.

FreeInkBook's four rules (never a DOM; arena-only memory; layout once,
render many via a `FIBP` page cache; host-testable end to end — see
`freeink-sdk/docs/freeink-book.md`) describe, more systematically, several
things `lib/Epub` already does piecemeal (arena word packing, streaming
parse). Its real advantages over `lib/Epub` as it stands today: runtime TTF
fonts (closes Gap 2 directly), a formal page-cache format with documented
memory profiles for PSRAM-less ESP32-C3 (~106 KB) vs PSRAM parts (~152 KB),
and KOReader-style XPath sync / char-offset universal locators
(`freeink-sdk` commit `34211716`, "Add character-offset bookmarks for
FreeInkBook").

**This is a big-bet option, not a quick win.** Swapping engines is a
project unto itself (new page-serialization format, new activity
integration, a full pass through `PORT_CHECKLIST.md`) and the submodule is
pinned 133 commits behind its own `origin/main` with only 2 of those touching
`FreeInkBook` itself (`032e7f6` M5 Paper Mono board support — irrelevant
here; `fdf246d` a `TtfFont`/glyph-cache perf pass — mildly relevant if this
is ever pursued). No `freeink-book-design.md` (which `freeink-book.md`
references as carrying "prior-art notes") exists at either the pinned
commit or `origin/main` — so no ready-made comparison document to lean on;
this file is the first place that comparison has been written down.
Recommendation: **don't start this now.** Track it as a known lever; revisit
if/when Gap 2 (fonts) and general engine maintenance load make a full
engine swap look cheaper than continuing to extend `lib/Epub` piecemeal.

## Explicitly NOT worth doing: porting CrossPoint's `97ee05a4` fix

CrossPoint has an unmerged branch,
`crosspoint/fix/handle-crashes-on-very-large-epub-chapters-#2256`
(commit `97ee05a4`), that caps buffered text at a flat 300 words
(`MAX_BUFFERED_TEXT_WORDS`) and switches `anchorData` from `std::vector` to
`std::deque` to reduce contiguous-heap-growth risk on very large chapters
(their comment: "Spotted when reading Intermezzo"). It is not merged into
CrossPoint's own `master` either — a single-commit orphan branch.

`lib/Epub` already has a superset of this: word- *and* byte-bounded
adaptive flushing (`DEFAULT_BUFFERED_WORDS_BEFORE_LAYOUT = 350`,
`DEFAULT_TEXT_RUN_BYTES_BEFORE_LAYOUT = 2048`, plus separate CSS/bionic/
guide-reading thresholds — `ChapterHtmlSlimParser.cpp:36-42`). Cherry-picking
`97ee05a4` would be a regression to a cruder mechanism, not an improvement.
The one piece of that fix genuinely absent here — `anchorData` is still
`std::vector<std::pair<std::string, uint16_t>>`, not a `std::deque` — is a
real but minor residual risk: it only matters for a single pathologically
anchor-dense chapter (anchors accumulate per-chapter-parse, not per-book),
and vector reallocation of a `<string,uint16_t>` pair is a much smaller
class of fragmentation event than the paragraph-text-buffer growth the fix
was actually chasing. Low priority; noted here so it isn't rediscovered
from scratch, not queued as an action item.

## Priority order for next session

1. **Verify Gap 1's fix compiles and renders correctly** once `-e default`
   or `-e simulator` builds are possible in this environment (or on a
   machine that has the toolchain/SDL2 already). Add the `.epub` fixture +
   host test described above.
2. **Gap 2 (embedded/runtime fonts)** — scope a `PORT_CHECKLIST.md`-style
   pass; FreeInkBook's `TtfFont`/`FontChain` is a concrete reference
   implementation to study, even if not adopted wholesale.
3. **Gap 3 (FreeInkBook adoption)** — track only; revisit when Gap 2's
   scoping work makes the size of a full engine swap clearer.
