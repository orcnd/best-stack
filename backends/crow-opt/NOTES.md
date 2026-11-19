# crow-opt notes (run 3)

Same framework setup as `backends/crow`: this target compiles `../crow/main.cpp` unchanged (Crow v1.3.5,
`CROW_DISABLE_STATIC_DIR`, `--threads` → `concurrency`). The startup line therefore still says
`bs-crow listening …`. Only the core it links differs:

| | `crow` | `crow-opt` |
|---|---|---|
| Core | `bs_core` (`shared/cpp`) | `bs_core_fast` (`shared/cpp-fast`) |
| DB layer | `shared/cpp/src/db.cpp` | same file, same SQL, same SQLite amalgamation flags |
| HTML | nlohmann view model → `escape_all` → inja | written directly into one reserved `std::string`, escaped while writing |
| JSON out | `nlohmann::json(...).dump()` | glaze v9.1.0, structs serialized directly |
| JSON in | `nlohmann::json::parse` | `glz::generic_u64` (keeps the integer/float split for validation) |
| Allocator | system malloc | mimalloc v3.7.0, static override |

## mimalloc override (macOS)

mimalloc is built with `MI_OVERRIDE=ON` (with its defaults `MI_OSX_ZONE=ON`, `MI_OSX_INTERPOSE=ON`) and the
**object file** `mimalloc-obj` (src/static.c) is linked into the executable, so the `malloc`/`free`/`new`/
`delete` definitions and the malloc-zone override can't be dropped by the linker.

Verified at startup (`shared/cpp-fast/src/alloc.cpp`, run from `bs::init`): one `malloc`, one `new[]` and
one `strdup` (an allocation made inside libSystem, i.e. through the default malloc zone) are checked with
`mi_is_in_heap_region()`. A warning is printed to stderr if any of them is not a mimalloc pointer;
`BS_ALLOC_CHECK=1` always prints the report:

```
$ BS_ALLOC_CHECK=1 build/backends/crow-opt/bs-crow-opt --port 18312 --db /tmp/x.sqlite --root .
mimalloc 3.7.0: malloc=mi new=mi libSystem-strdup=mi strdup-zone="DefaultMallocZone"
```

(The zone keeps the name `DefaultMallocZone`: mimalloc patches the default zone rather than registering a
separately named one as the default, but the pointers it returns are mimalloc's.) The same three checks are a
unit test in `bs_fast_tests`.

## Equivalence

`node bench/parity/golden.ts crow crow-opt` → 152/152 equal, all 132 non-JSON bodies byte-identical.
