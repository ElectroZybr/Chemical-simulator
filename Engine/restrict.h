#pragma once
// Исправление бага: прямой `#pragma GCC ivdep` давал MSVC warning C4068.
// Вместо него call sites используют один macro, зависящий от compiler.
#if defined(_MSC_VER)
#define RESTRICT __restrict
#define LATTICELAB_IVDEP __pragma(loop(ivdep))
#elif defined(__GNUC__) || defined(__clang__)
#define RESTRICT __restrict__
#define LATTICELAB_IVDEP _Pragma("GCC ivdep")
#else
#define RESTRICT
#define LATTICELAB_IVDEP
#endif
