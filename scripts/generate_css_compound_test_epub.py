#!/usr/bin/env python3
"""
Generate a minimal test EPUB that targets the two-class compound CSS
selector fix in lib/Epub/Epub/css/CssParser.cpp (splitTwoClassSelector /
classTokenLess / buildTwoClassKey).

The stylesheet declares compound selectors in one class order; the markup
applies the matching classes in the OPPOSITE order, on purpose, to exercise
buildTwoClassKey's order-independence (the exact property that was broken
before the fix: rules were parsed and stored, but never matched at resolve
time because parse-time and resolve-time selectors were compared as literal
strings instead of a canonical, order-independent key).

Cases covered:
  1. Bare two-class compound: ".a.b" in CSS vs class="b a" in HTML.
  2. Tag-qualified two-class compound: "p.warn.big" in CSS vs
     class="big warn" in HTML, on a <p> (tag matters for this form).
  3. A single-class control (".solo") to confirm ordinary single-class
     rules still apply -- i.e. the fixture isn't accidentally exercising
     only the new code path and silently passing for the wrong reason.
  4. A three-class selector in the CSS source (".x.y.z") which the parser
     is documented to silently ignore (unsupported) -- included so the
     fixture also demonstrates that unsupported selectors don't crash
     parsing of the rules around them.
"""

import zipfile
from pathlib import Path

OUTPUT_DIR = Path(__file__).parent.parent / "test" / "epubs"
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

CSS = """.m.n { font-weight: bold; }
p.m.n { font-weight: normal; }
.m.o { font-weight: bold; }
.solo {
  font-style: italic;
}
.a.b {
  font-weight: bold;
}
p.warn.big {
  font-weight: bold;
}
.x.y.z {
  font-weight: bold;
}
"""

CHAPTER = """<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
  <title>Compound Selector Test</title>
  <link rel="stylesheet" type="text/css" href="style.css"/>
</head>
<body>
  <h1>Compound Class Selector Test</h1>

  <p class="m n o">Tag-qualified compound beats bare compound: plain.</p>
  <p class="o n m">Same specificity check with reversed HTML classes: plain.</p>
  <div class="m n o">Bare compound applies without the p tag: bold.</div>

  <p class="solo">Control: single-class rule, should be italic.</p>

  <p class="b a">Bare compound, CSS declares ".a.b", markup lists "b a"
  (reversed) -- should be bold if compound matching is order-independent.</p>

  <p class="big warn">Tag-qualified compound, CSS declares "p.warn.big",
  markup lists "big warn" (reversed) -- should be bold on this p element.</p>

  <div class="big warn">Same two classes, reversed order, but on a div
  instead of a p -- the tag-qualified rule "p.warn.big" must NOT match here
  (should stay plain, unlike the paragraph above).</div>

  <p class="x y z">Three-class selector in the source CSS (".x.y.z") is
  documented as unsupported and silently ignored -- should stay plain, and
  parsing the rest of the sheet should not be disturbed by it.</p>

  <p>Plain paragraph, no class -- should stay plain, confirming the
  compound rules above are not leaking onto unrelated elements.</p>
</body>
</html>"""


def create_epub(filename: Path) -> None:
    with zipfile.ZipFile(filename, "w", zipfile.ZIP_DEFLATED) as epub:
        epub.writestr("mimetype", "application/epub+zip", compress_type=zipfile.ZIP_STORED)

        epub.writestr(
            "META-INF/container.xml",
            """<?xml version="1.0"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
  <rootfiles>
    <rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/>
  </rootfiles>
</container>""",
        )

        epub.writestr("OEBPS/style.css", CSS)
        epub.writestr("OEBPS/chapter0.xhtml", CHAPTER)

        epub.writestr(
            "OEBPS/content.opf",
            """<?xml version="1.0"?>
<package version="2.0" xmlns="http://www.idpf.org/2007/opf" unique-identifier="bookid">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
    <dc:title>CSS Compound Selector Test</dc:title>
    <dc:creator>x4-merge-v2 Test Generator</dc:creator>
    <dc:language>en</dc:language>
    <dc:identifier id="bookid">test-css-compound-001</dc:identifier>
  </metadata>
  <manifest>
    <item id="style" href="style.css" media-type="text/css"/>
    <item id="chapter0" href="chapter0.xhtml" media-type="application/xhtml+xml"/>
    <item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>
  </manifest>
  <spine toc="ncx">
    <itemref idref="chapter0"/>
  </spine>
</package>""",
        )

        epub.writestr(
            "OEBPS/toc.ncx",
            """<?xml version="1.0"?>
<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1">
  <head>
    <meta name="dtb:uid" content="test-css-compound-001"/>
  </head>
  <docTitle><text>CSS Compound Selector Test</text></docTitle>
  <navMap>
    <navPoint id="navPoint-1" playOrder="1">
      <navLabel><text>Compound Selector Test</text></navLabel>
      <content src="chapter0.xhtml"/>
    </navPoint>
  </navMap>
</ncx>""",
        )


if __name__ == "__main__":
    out = OUTPUT_DIR / "test_css_compound_selectors.epub"
    create_epub(out)
    print(f"Created: {out}")
