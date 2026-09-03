# libsodium

**Version:** 1.0.20 (stable)
**Source archive:** libsodium-1.0.20-stable-msvc.zip
**SHA-256:** EBAA204FDFCEDC51DC1EE1BBD03C8D552A14B3372F87F94E44C71A8533F77DF4
**Obtained:** https://download.libsodium.org/libsodium/releases/
**Variant:** static, /MT, v143 toolset

Only the headers and the static `.lib` files are vendored. Do not modify anything under this directory — replace it wholesale if upgrading.

Consuming projects link `libsodium.lib` from `lib/x86/` or `lib/x64/` matching their platform.
