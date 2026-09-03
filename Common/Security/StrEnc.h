#pragma once
#include <cstdint>
#include <cstring>
#include <array>

// Compile-time XOR string obfuscation.
// Usage:  const char* s = ESTR("hello world");  -- decrypts at runtime, once.
//
// The key is derived from __LINE__ + __COUNTER__ so each use gets a
// different key.  Useful for hiding literal strings from static scanners.

namespace hac { namespace security {

template<std::size_t N, uint8_t Key>
struct XorStr {
    char data[N];
    constexpr XorStr(const char (&src)[N]) {
        for (std::size_t i = 0; i < N; ++i)
            data[i] = src[i] ^ static_cast<char>((Key + i) & 0xFF);
    }
    // Decrypts in-place into a stack buffer.
    void decrypt(char* out) const {
        for (std::size_t i = 0; i < N; ++i)
            out[i] = data[i] ^ static_cast<char>((Key + i) & 0xFF);
    }
};

}} // namespace hac::security

// ESTR("literal") — evaluates to const char* of a stack-decrypted copy.
// The XorStr is stored in read-only data; the plain text only lives on stack.
// ESTR("literal") — evaluates to const char* of a thread-local decrypted copy.
// thread_local ensures each thread gets its own _buf so concurrent callers
// cannot corrupt each other's plaintext.
#define ESTR(s)                                                              \
    ([]{                                                                     \
        static constexpr hac::security::XorStr<sizeof(s),                   \
            static_cast<uint8_t>((__LINE__ * 31 + __COUNTER__) & 0xFF)>     \
            _enc(s);                                                         \
        thread_local char _buf[sizeof(s)];                                   \
        _enc.decrypt(_buf);                                                  \
        return static_cast<const char*>(_buf);                               \
    }())
