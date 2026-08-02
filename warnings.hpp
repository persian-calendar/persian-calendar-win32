#pragma once

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

// MARK: Compiler detection

#ifdef __clang__
#define IB_COMPILER_CLANG 1
#elifdef __GNUC__
#define IB_COMPILER_GCC 1
#elifdef _MSC_VER
#define IB_COMPILER_MSVC 1
#else
#define IB_COMPILER_UNKNOWN 1
#endif

// useful macros for disabling warnings in a cross-platform way
// usage:
// IB_WARNING_PUSH
// IB_WARNING_DISABLE_GCC_CLANG("-Wfloat-equal")
// IB_WARNING_DISABLE_MSVC(4800)
// code that triggers the warning
// IB_WARNING_POP
//
// or, using the compiler-specific push/pop pairs:
// IB_WARNING_DISABLE_CLANG_PUSH("-Wfloat-equal")
// code
// IB_WARNING_DISABLE_CLANG_POP

#define IB_PRAGMA_TO_STR(x) _Pragma(#x)

#ifdef IB_COMPILER_CLANG

#define IB_WARNING_PUSH IB_PRAGMA_TO_STR(clang diagnostic push)
#define IB_WARNING_POP IB_PRAGMA_TO_STR(clang diagnostic pop)

#define IB_WARNING_DISABLE_CLANG(warning) IB_PRAGMA_TO_STR(clang diagnostic ignored warning)
#define IB_WARNING_DISABLE_CLANG_PUSH(warning) IB_WARNING_PUSH IB_WARNING_DISABLE_CLANG(warning)
#define IB_WARNING_DISABLE_CLANG_POP IB_WARNING_POP

#define IB_WARNING_DISABLE_GCC(warning)
#define IB_WARNING_DISABLE_GCC_PUSH(warning)
#define IB_WARNING_DISABLE_GCC_POP

#define IB_WARNING_DISABLE_MSVC(warning)
#define IB_WARNING_DISABLE_MSVC_PUSH(warning)
#define IB_WARNING_DISABLE_MSVC_POP

#define IB_WARNING_DISABLE_GCC_CLANG(warning) IB_WARNING_DISABLE_CLANG(warning)
#define IB_WARNING_DISABLE_GCC_CLANG_PUSH(warning) IB_WARNING_PUSH IB_WARNING_DISABLE_GCC_CLANG(warning)
#define IB_WARNING_DISABLE_GCC_CLANG_POP IB_WARNING_POP

#if defined(__has_warning) && __has_warning("-Wdeprecated-pragma")
#define IB_WARNING_DISABLE_DEPRECATED_MACRO IB_WARNING_DISABLE_CLANG("-Wdeprecated-pragma")
#else
#define IB_WARNING_DISABLE_DEPRECATED_MACRO
#endif

#define IB_WARNING_DISABLE_DEPRECATED IB_WARNING_DISABLE_CLANG("-Wdeprecated-declarations") IB_WARNING_DISABLE_DEPRECATED_MACRO

#elifdef IB_COMPILER_GCC

#define IB_WARNING_PUSH IB_PRAGMA_TO_STR(GCC diagnostic push)
#define IB_WARNING_POP IB_PRAGMA_TO_STR(GCC diagnostic pop)

#define IB_WARNING_DISABLE_CLANG(warning)
#define IB_WARNING_DISABLE_CLANG_PUSH(warning)
#define IB_WARNING_DISABLE_CLANG_POP

#define IB_WARNING_DISABLE_GCC(warning) IB_PRAGMA_TO_STR(GCC diagnostic ignored warning)
#define IB_WARNING_DISABLE_GCC_PUSH(warning) IB_WARNING_PUSH IB_WARNING_DISABLE_GCC(warning)
#define IB_WARNING_DISABLE_GCC_POP IB_WARNING_POP

#define IB_WARNING_DISABLE_MSVC(warning)
#define IB_WARNING_DISABLE_MSVC_PUSH(warning)
#define IB_WARNING_DISABLE_MSVC_POP

#define IB_WARNING_DISABLE_GCC_CLANG(warning) IB_WARNING_DISABLE_GCC(warning)
#define IB_WARNING_DISABLE_GCC_CLANG_PUSH(warning) IB_WARNING_PUSH IB_WARNING_DISABLE_GCC_CLANG(warning)
#define IB_WARNING_DISABLE_GCC_CLANG_POP IB_WARNING_POP

#define IB_WARNING_DISABLE_DEPRECATED IB_WARNING_DISABLE_GCC("-Wdeprecated-declarations")

#elifdef IB_COMPILER_MSVC

#undef IB_PRAGMA_TO_STR

#define IB_WARNING_PUSH __pragma(warning(push))
#define IB_WARNING_POP __pragma(warning(pop))

#define IB_WARNING_DISABLE_CLANG(warning)
#define IB_WARNING_DISABLE_CLANG_PUSH(warning)
#define IB_WARNING_DISABLE_CLANG_POP

#define IB_WARNING_DISABLE_GCC(warning)
#define IB_WARNING_DISABLE_GCC_PUSH(warning)
#define IB_WARNING_DISABLE_GCC_POP

#define IB_WARNING_DISABLE_MSVC(warning) __pragma(warning(disable : warning))
#define IB_WARNING_DISABLE_MSVC_PUSH(warning) IB_WARNING_PUSH IB_WARNING_DISABLE_MSVC(warning)
#define IB_WARNING_DISABLE_MSVC_POP IB_WARNING_POP

#define IB_WARNING_DISABLE_GCC_CLANG(warning)
#define IB_WARNING_DISABLE_GCC_CLANG_PUSH(warning)
#define IB_WARNING_DISABLE_GCC_CLANG_POP

#define IB_WARNING_DISABLE_DEPRECATED IB_WARNING_DISABLE_MSVC(4996) IB_WARNING_DISABLE_MSVC(4995)

#else

#define IB_WARNING_PUSH
#define IB_WARNING_POP

#define IB_WARNING_DISABLE_CLANG(warning)
#define IB_WARNING_DISABLE_CLANG_PUSH(warning)
#define IB_WARNING_DISABLE_CLANG_POP

#define IB_WARNING_DISABLE_GCC(warning)
#define IB_WARNING_DISABLE_GCC_PUSH(warning)
#define IB_WARNING_DISABLE_GCC_POP

#define IB_WARNING_DISABLE_MSVC(warning)
#define IB_WARNING_DISABLE_MSVC_PUSH(warning)
#define IB_WARNING_DISABLE_MSVC_POP

#define IB_WARNING_DISABLE_GCC_CLANG(warning)
#define IB_WARNING_DISABLE_GCC_CLANG_PUSH(warning)
#define IB_WARNING_DISABLE_GCC_CLANG_POP

#define IB_WARNING_DISABLE_DEPRECATED

#endif

// NOLINTEND(cppcoreguidelines-macro-usage)
