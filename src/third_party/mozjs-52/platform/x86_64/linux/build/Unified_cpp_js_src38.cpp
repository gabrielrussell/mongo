#define MOZ_UNIFIED_BUILD
#include "wasm/WasmBinaryToText.cpp"
#ifdef PL_ARENA_CONST_ALIGN_MASK
#error "wasm/WasmBinaryToText.cpp uses PL_ARENA_CONST_ALIGN_MASK, so it cannot be built in unified mode."
#undef PL_ARENA_CONST_ALIGN_MASK
#endif
#ifdef INITGUID
#error "wasm/WasmBinaryToText.cpp defines INITGUID, so it cannot be built in unified mode."
#undef INITGUID
#endif
#include "wasm/WasmCode.cpp"
#ifdef PL_ARENA_CONST_ALIGN_MASK
#error "wasm/WasmCode.cpp uses PL_ARENA_CONST_ALIGN_MASK, so it cannot be built in unified mode."
#undef PL_ARENA_CONST_ALIGN_MASK
#endif
#ifdef INITGUID
#error "wasm/WasmCode.cpp defines INITGUID, so it cannot be built in unified mode."
#undef INITGUID
#endif
#include "wasm/WasmCompartment.cpp"
#ifdef PL_ARENA_CONST_ALIGN_MASK
#error "wasm/WasmCompartment.cpp uses PL_ARENA_CONST_ALIGN_MASK, so it cannot be built in unified mode."
#undef PL_ARENA_CONST_ALIGN_MASK
#endif
#ifdef INITGUID
#error "wasm/WasmCompartment.cpp defines INITGUID, so it cannot be built in unified mode."
#undef INITGUID
#endif
#include "wasm/WasmCompile.cpp"
#ifdef PL_ARENA_CONST_ALIGN_MASK
#error "wasm/WasmCompile.cpp uses PL_ARENA_CONST_ALIGN_MASK, so it cannot be built in unified mode."
#undef PL_ARENA_CONST_ALIGN_MASK
#endif
#ifdef INITGUID
#error "wasm/WasmCompile.cpp defines INITGUID, so it cannot be built in unified mode."
#undef INITGUID
#endif
#include "wasm/WasmFrameIterator.cpp"
#ifdef PL_ARENA_CONST_ALIGN_MASK
#error "wasm/WasmFrameIterator.cpp uses PL_ARENA_CONST_ALIGN_MASK, so it cannot be built in unified mode."
#undef PL_ARENA_CONST_ALIGN_MASK
#endif
#ifdef INITGUID
#error "wasm/WasmFrameIterator.cpp defines INITGUID, so it cannot be built in unified mode."
#undef INITGUID
#endif
#include "wasm/WasmGenerator.cpp"
#ifdef PL_ARENA_CONST_ALIGN_MASK
#error "wasm/WasmGenerator.cpp uses PL_ARENA_CONST_ALIGN_MASK, so it cannot be built in unified mode."
#undef PL_ARENA_CONST_ALIGN_MASK
#endif
#ifdef INITGUID
#error "wasm/WasmGenerator.cpp defines INITGUID, so it cannot be built in unified mode."
#undef INITGUID
#endif