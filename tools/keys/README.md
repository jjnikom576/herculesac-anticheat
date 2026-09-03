# Signing keys

- `dev.public.key.hex` — checked in. Matches `PublicKey.h`'s default `HAC_PUBKEY_HEX`.
  Used for developer builds and CI. Never used in production.
- `*.private.key` / `*.private.key.hex` — never committed. Held by the release engineer.

Generate a new keypair with `tools/sign-manifest\sign-manifest.exe --gen-keypair`
(built in Task 9). The tool writes `hac-<label>.private.key` (raw 64 bytes) and
`hac-<label>.public.key.hex` (64 hex chars) into the current directory.

To swap keys for a build, define `HAC_PUBKEY_HEX="..."` in the msbuild command line.
