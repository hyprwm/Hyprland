#pragma once

#include <type_traits>

#define ULL unsigned long long
#define LD  long double

constexpr ULL operator""_kB(const ULL BYTES) {
    return BYTES * 1024;
}
constexpr ULL operator""_MB(const ULL BYTES) {
    return BYTES * 1024 * 1024;
}
constexpr ULL operator""_GB(const ULL BYTES) {
    return BYTES * 1024 * 1024 * 1024;
}
constexpr ULL operator""_TB(const ULL BYTES) {
    return BYTES * 1024 * 1024 * 1024 * 1024;
}
constexpr LD operator""_kB(const LD BYTES) {
    return BYTES * 1024;
}
constexpr LD operator""_MB(const LD BYTES) {
    return BYTES * 1024 * 1024;
}
constexpr LD operator""_GB(const LD BYTES) {
    return BYTES * 1024 * 1024 * 1024;
}
constexpr LD operator""_TB(const LD BYTES) {
    return BYTES * 1024 * 1024 * 1024 * 1024;
}

//NOLINTBEGIN
template <typename T>
using internal_hl_acceptable_byte_operation_type = typename std::enable_if<std::is_trivially_constructible<T, ULL>::value || std::is_trivially_constructible<T, LD>::value>::type;

template <typename X, typename = internal_hl_acceptable_byte_operation_type<X>>
constexpr X kBtoBytes(const X kB) {
    return kB * 1024;
}
template <typename X, typename = internal_hl_acceptable_byte_operation_type<X>>
constexpr X MBtoBytes(const X MB) {
    return MB * 1024 * 1024;
}
template <typename X, typename = internal_hl_acceptable_byte_operation_type<X>>
constexpr X GBtoBytes(const X GB) {
    return GB * 1024 * 1024 * 1024;
}
template <typename X, typename = internal_hl_acceptable_byte_operation_type<X>>
constexpr X TBtoBytes(const X TB) {
    return TB * 1024 * 1024 * 1024 * 1024;
}
//NOLINTEND

#undef ULL
#undef LD