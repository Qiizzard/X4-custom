#ifdef SIMULATOR
#include "SimulatorCssTest.h"

#include <Epub/css/CssParser.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

namespace {
bool checkStyles(const CssParser& parser) {
  return parser.resolveStyle("p", "m n o").fontWeight == CssFontWeight::Normal &&
         parser.resolveStyle("p", "o n m").fontWeight == CssFontWeight::Normal &&
         parser.resolveStyle("div", "m n o").fontWeight == CssFontWeight::Bold &&
         parser.resolveStyle("p", "b a").fontWeight == CssFontWeight::Bold &&
         parser.resolveStyle("p", "a b").fontWeight == CssFontWeight::Bold &&
         parser.resolveStyle("p", "big warn").fontWeight == CssFontWeight::Bold &&
         parser.resolveStyle("div", "big warn").fontWeight == CssFontWeight::Normal &&
         parser.resolveStyle("p", "a").fontWeight == CssFontWeight::Normal &&
         parser.resolveStyle("p", "solo").fontStyle == CssFontStyle::Italic &&
         parser.resolveStyle("p", "x y z").fontWeight == CssFontWeight::Normal;
}
}  // namespace

bool verifySimulatorCssCacheContract() {
  // The smoke runner provides a disposable fs_; never run against a user's SD.
  static constexpr char css[] =
      ".m.n {font-weight:bold;} p.m.n {font-weight:normal;} .m.o {font-weight:bold;} "
      ".b.a {font-weight:bold;} p.warn.big {font-weight:bold;} "
      ".solo {font-style:italic;} .x.y.z {font-weight:bold;}";
  FsFile source;
  if (!Storage.openFileForWrite("SMOKE", "/compound-test.css", source)) return false;
  const bool wrote = source.write(css, sizeof(css) - 1) == sizeof(css) - 1;
  source.close();
  if (!wrote) {
    LOG_ERR("SMOKE", "Cannot write compound CSS fixture");
    return false;
  }
  // Parser containers exceed the small stack budget; ownership is local and
  // released before the activity walk starts.
  auto parser = makeUniqueNoThrow<CssParser>("/");
  if (!parser) {
    LOG_ERR("SMOKE", "Cannot allocate CSS parser");
    return false;
  }
  if (!Storage.openFileForRead("SMOKE", "/compound-test.css", source)) return false;
  const bool parsed = parser->loadFromStream(source);
  source.close();
  if (!parsed || !checkStyles(*parser) || !parser->saveToCache()) {
    LOG_ERR("SMOKE", "Cold compound CSS contract failed");
    return false;
  }
  parser->clear();
  if (!parser->loadFromCache() || !checkStyles(*parser)) {
    LOG_ERR("SMOKE", "Cached compound CSS contract failed");
    return false;
  }
  // Mutate only the version byte of an otherwise valid cache, proving old
  // versions are rejected rather than merely testing a truncated file.
  parser->clear();
  FsFile cache = Storage.open("/css_rules.cache", O_RDWR);
  if (!cache) {
    LOG_ERR("SMOKE", "Cannot open CSS cache for version regression");
    return false;
  }
  const uint8_t previousVersion = 16;
  const bool changed = cache.seekSet(sizeof(uint32_t)) && cache.write(&previousVersion, 1) == 1;
  cache.close();
  if (!changed || parser->loadFromCache()) {
    LOG_ERR("SMOKE", "Version-16 CSS cache was not rejected");
    return false;
  }
  parser->deleteCache();
  Storage.remove("/compound-test.css");
  LOG_INF("SMOKE", "Cold and cached compound CSS contract passed");
  return true;
}
#endif
