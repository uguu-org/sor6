// Common definitions.
#ifndef COMMON_H_
#define COMMON_H_

#include<assert.h>
#include<stdint.h>
#include<stdlib.h>

// Screen width and height in pixels.
//
// These are same as LCD_COLUMNS and LCD_ROWS from pd_api_gfx.h, but we
// define our own constants here to avoid dependency on Playdate SDK within
// our own library functions when building for simulator.  This makes
// the file easier to test.
#define SCREEN_WIDTH    400
#define SCREEN_HEIGHT   240

// Screen stride in bytes.
//
// This is same as LCD_ROWSIZE from pd_api_gfx.h.
#define SCREEN_STRIDE   52

// Branch prediction hints.
//
// https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html#index-_005f_005fbuiltin_005fexpect
// https://llvm.org/docs/BranchWeightMetadata.html#builtin-expect
#if __has_builtin(__builtin_expect)
   #define LIKELY(x)    __builtin_expect(!!(x), 1)
   #define UNLIKELY(x)  __builtin_expect(!!(x), 0)
#else
   #define LIKELY(x)    x
   #define UNLIKELY(x)  x
#endif

// Unreachable code.
//
// https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html#index-_005f_005fbuiltin_005funreachable
// https://clang.llvm.org/docs/LanguageExtensions.html#builtin-unreachable
#if __has_builtin(__builtin_unreachable)
   #define UNREACHABLE()   __builtin_unreachable()
#else
   #define UNREACHABLE()   assert(0)
#endif

// Generate a random integer in the range of 0..max.
// https://c-faq.com/lib/randrange.html
#define RAND(max)    ((int)( rand() / ((unsigned)RAND_MAX / (max + 1U) + 1U) ))

// Syntactic sugar, generate random number in the range of min..max.
#define RAND_RANGE(min, max)  (RAND((max) - (min)) + (min))

#endif  // COMMON_H_
