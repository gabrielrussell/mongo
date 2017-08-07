#define MOZ_UNIFIED_BUILD
#include "vm/UbiNodeShortestPaths.cpp"
#ifdef PL_ARENA_CONST_ALIGN_MASK
#error "vm/UbiNodeShortestPaths.cpp uses PL_ARENA_CONST_ALIGN_MASK, so it cannot be built in unified mode."
#undef PL_ARENA_CONST_ALIGN_MASK
#endif
#ifdef INITGUID
#error "vm/UbiNodeShortestPaths.cpp defines INITGUID, so it cannot be built in unified mode."
#undef INITGUID
#endif
#include "vm/UnboxedObject.cpp"
#ifdef PL_ARENA_CONST_ALIGN_MASK
#error "vm/UnboxedObject.cpp uses PL_ARENA_CONST_ALIGN_MASK, so it cannot be built in unified mode."
#undef PL_ARENA_CONST_ALIGN_MASK
#endif
#ifdef INITGUID
#error "vm/UnboxedObject.cpp defines INITGUID, so it cannot be built in unified mode."
#undef INITGUID
#endif
#include "vm/Unicode.cpp"
#ifdef PL_ARENA_CONST_ALIGN_MASK
#error "vm/Unicode.cpp uses PL_ARENA_CONST_ALIGN_MASK, so it cannot be built in unified mode."
#undef PL_ARENA_CONST_ALIGN_MASK
#endif
#ifdef INITGUID
#error "vm/Unicode.cpp defines INITGUID, so it cannot be built in unified mode."
#undef INITGUID
#endif
#include "vm/Value.cpp"
#ifdef PL_ARENA_CONST_ALIGN_MASK
#error "vm/Value.cpp uses PL_ARENA_CONST_ALIGN_MASK, so it cannot be built in unified mode."
#undef PL_ARENA_CONST_ALIGN_MASK
#endif
#ifdef INITGUID
#error "vm/Value.cpp defines INITGUID, so it cannot be built in unified mode."
#undef INITGUID
#endif
#include "vm/WeakMapPtr.cpp"
#ifdef PL_ARENA_CONST_ALIGN_MASK
#error "vm/WeakMapPtr.cpp uses PL_ARENA_CONST_ALIGN_MASK, so it cannot be built in unified mode."
#undef PL_ARENA_CONST_ALIGN_MASK
#endif
#ifdef INITGUID
#error "vm/WeakMapPtr.cpp defines INITGUID, so it cannot be built in unified mode."
#undef INITGUID
#endif
#include "vm/Xdr.cpp"
#ifdef PL_ARENA_CONST_ALIGN_MASK
#error "vm/Xdr.cpp uses PL_ARENA_CONST_ALIGN_MASK, so it cannot be built in unified mode."
#undef PL_ARENA_CONST_ALIGN_MASK
#endif
#ifdef INITGUID
#error "vm/Xdr.cpp defines INITGUID, so it cannot be built in unified mode."
#undef INITGUID
#endif