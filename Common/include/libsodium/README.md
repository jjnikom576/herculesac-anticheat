# libsodium

**Version:** 1.0.20 (stable)
**Source archive:** libsodium-1.0.20-stable-msvc.zip
**SHA-256:** EBAA204FDFCEDC51DC1EE1BBD03C8D552A14B3372F87F94E44C71A8533F77DF4
**Obtained:** https://download.libsodium.org/libsodium/releases/
**Variant:** static, /MT, v143 toolset

Only the headers and the static `.lib` files are vendored. Do not modify anything under this directory — replace it wholesale if upgrading.

Consuming projects link `libsodium.lib` from `lib/x86/` or `lib/x64/` matching their platform.

## Consumer requirements

Consuming projects **must** define the preprocessor macro `SODIUM_STATIC`
when linking against these static libraries. Without it, `sodium/export.h`
declares symbols as `__declspec(dllimport)` and the link fails with
unresolved externals such as `__imp_sodium_init`.

- Command line: pass `/DSODIUM_STATIC` to `cl.exe`.
- MSBuild vcxproj: add `SODIUM_STATIC` to `<PreprocessorDefinitions>`.

## Known upstream anomaly (do not patch)

`include/sodium.h` lines 30-31 and 33-34 duplicate the includes for
`crypto_kdf_hkdf_sha256.h` and `crypto_kdf_hkdf_sha512.h`. This is present
verbatim in the upstream libsodium 1.0.20 stable MSVC package. The individual
header include guards make it harmless (single translation unit, no
redefinition). Do NOT edit the vendored header to fix it — the vendoring
convention above forbids local patches. If a future upstream release cleans
this up, replace the whole tree per the update procedure above.
