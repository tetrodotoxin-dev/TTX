// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tests/semantic/measurement.hpp"

#include <new>
#include <stddef.h>

#include "perimortem/core/bibliotheca.hpp"

using Perimortem::Core::Bibliotheca;
using Validation::FlowTests::Measurement;

Measurement* Measurement::active = nullptr;
Measurement::Measurement()
    : previous(active), checkouts(Bibliotheca::check_out_requests()) {
  active = this;
}
Measurement::~Measurement() {
  stop();
}
auto Measurement::stop() -> void {
  if (active == this) {
    allocations += Bibliotheca::check_out_requests() - checkouts;
    active = previous;
  }
}
auto Measurement::allocation() -> void {
  for (auto* interval = active; interval; interval = interval->previous) {
    ++interval->allocations;
  }
}
auto Measurement::copy() -> void {
  for (auto* interval = active; interval; interval = interval->previous) {
    ++interval->copies;
  }
}

//
// WARNING: The below is compiler jank and only sanctioned for test use!
//
// These wrappers observe direct references from this executable. C++ new
// needs its own entries because libstdc++ calls malloc inside its shared
// library, beyond the references rewritten by the executable linker.
//

extern "C" {
void* __real_memmove(void*, const void*, size_t);
void* __wrap_memmove(void* destination, const void* source, size_t size) {
  Measurement::copy();
  return __real_memmove(destination, source, size);
}
#if __has_feature(address_sanitizer)
// ASan rewrites memory intrinsics before ordinary wrapping. Observe that entry
// too so a sanitizer run checks the same copy count instead of skipping it.
void* __real___asan_memmove(void*, const void*, size_t);
void* __wrap___asan_memmove(
    void* destination,
    const void* source,
    size_t size) {
  Measurement::copy();
  return __real___asan_memmove(destination, source, size);
}
#endif
void* __real_malloc(size_t);
void* __real_calloc(size_t, size_t);
void* __real_realloc(void*, size_t);
void* __real_aligned_alloc(size_t, size_t);
int __real_posix_memalign(void**, size_t, size_t);
void* __wrap_malloc(size_t size) {
  Measurement::allocation();
  return __real_malloc(size);
}
void* __wrap_calloc(size_t count, size_t size) {
  Measurement::allocation();
  return __real_calloc(count, size);
}
void* __wrap_realloc(void* pointer, size_t size) {
  Measurement::allocation();
  return __real_realloc(pointer, size);
}
void* __wrap_aligned_alloc(size_t alignment, size_t size) {
  Measurement::allocation();
  return __real_aligned_alloc(alignment, size);
}
int __wrap_posix_memalign(void** pointer, size_t alignment, size_t size) {
  Measurement::allocation();
  return __real_posix_memalign(pointer, alignment, size);
}

void* __real__Znwm(size_t);
void* __wrap__Znwm(size_t size) {
  Measurement::allocation();
  return __real__Znwm(size);
}
void* __real__Znam(size_t);
void* __wrap__Znam(size_t size) {
  Measurement::allocation();
  return __real__Znam(size);
}
void* __real__ZnwmSt11align_val_t(size_t, std::align_val_t);
void* __wrap__ZnwmSt11align_val_t(size_t size, std::align_val_t alignment) {
  Measurement::allocation();
  return __real__ZnwmSt11align_val_t(size, alignment);
}
void* __real__ZnamSt11align_val_t(size_t, std::align_val_t);
void* __wrap__ZnamSt11align_val_t(size_t size, std::align_val_t alignment) {
  Measurement::allocation();
  return __real__ZnamSt11align_val_t(size, alignment);
}
}
