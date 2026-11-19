// mimalloc is linked as an object file built with MI_OVERRIDE (static override). This checks at startup
// that it really serves every allocation path: malloc from our code, operator new, and malloc called
// from inside a system dylib (strdup in libSystem, which goes through the default malloc zone).
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <malloc/malloc.h>
#include <mimalloc.h>

#include "bsf/alloc.hpp"

namespace bsf {

bool mimalloc_active(bool print) {
  void* p = std::malloc(48);
  auto* q = new char[48];
  char* s = ::strdup("libSystem allocation");
  const bool in_malloc = mi_is_in_heap_region(p), in_new = mi_is_in_heap_region(q), in_sys = mi_is_in_heap_region(s);
  malloc_zone_t* zone = malloc_zone_from_ptr(s);  // "mimalloc" when the zone override is registered
  const char* zone_name = zone && zone->zone_name ? zone->zone_name : "?";
  std::free(p);
  delete[] q;
  std::free(s);
  const bool ok = in_malloc && in_new && in_sys;
  if (print || !ok) {
    const int v = mi_version();  // major*10000 + minor*100 + patch
    std::fprintf(stderr, "mimalloc %d.%d.%d: malloc=%s new=%s libSystem-strdup=%s strdup-zone=\"%s\"%s\n", v / 10000,
                 (v / 100) % 100, v % 100, in_malloc ? "mi" : "system", in_new ? "mi" : "system",
                 in_sys ? "mi" : "system", zone_name, ok ? "" : "  WARNING: override not active");
  }
  return ok;
}

}  // namespace bsf
