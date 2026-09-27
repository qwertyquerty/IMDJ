#pragma once

#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
#define IMDJ_DENORMALS_X86 1
#include <pmmintrin.h>
#include <xmmintrin.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#define IMDJ_DENORMALS_ARM64 1
#endif

namespace imdj {

class ScopedNoDenormals {
public:
    ScopedNoDenormals()
    {
#if defined(IMDJ_DENORMALS_X86)
        previous_ = _mm_getcsr();
        _mm_setcsr(previous_ | 0x8040);
#elif defined(IMDJ_DENORMALS_ARM64)
        __asm__ __volatile__("mrs %0, fpcr" : "=r"(previous_));
        __asm__ __volatile__("msr fpcr, %0" ::"r"(previous_ | (1ull << 24)));
#endif
    }

    ~ScopedNoDenormals()
    {
#if defined(IMDJ_DENORMALS_X86)
        _mm_setcsr(previous_);
#elif defined(IMDJ_DENORMALS_ARM64)
        __asm__ __volatile__("msr fpcr, %0" ::"r"(previous_));
#endif
    }

    ScopedNoDenormals(const ScopedNoDenormals&) = delete;
    ScopedNoDenormals& operator=(const ScopedNoDenormals&) = delete;

private:
#if defined(IMDJ_DENORMALS_X86)
    unsigned int previous_ = 0;
#elif defined(IMDJ_DENORMALS_ARM64)
    unsigned long long previous_ = 0;
#endif
};

} // namespace imdj
