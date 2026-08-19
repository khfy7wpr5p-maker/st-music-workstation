# Project Core Identity Allocation — Entropy Trust Hardening

Status: normative companion to `docs/PROJECT_CORE_IDENTITY_ALLOCATION.md`

`IdentityEntropySource::read()` returning `success` with `bytes_written == 16` is a contract assertion from the injected adapter. The Core allocator can validate byte count and candidate shape, but it cannot independently prove that the 16 bytes came from a cryptographically secure source or that every reported byte was freshly produced by that adapter.

Therefore production composition must fail closed unless the injected entropy implementation is a separately reviewed operating-system CSPRNG adapter for the active platform. Test, scripted, deterministic, unreviewed, short-writing, partially-writing, or weak-random implementations are not production substitutes even when they report `success` and 16 bytes.

The platform adapter review must prove at minimum:

- the selected OS API is intended for cryptographically secure random generation;
- a successful adapter call fills the complete 16-byte destination before reporting success;
- partial/failed OS calls are surfaced as failure rather than padded, reused, or silently retried through a weaker source;
- no wall-clock, process ID, machine ID, `rand()`, framework pseudo-random default, cached seed, network service, or deterministic fallback is used;
- the adapter exposes only the ST-owned `IdentityEntropySource` boundary to Project Core;
- production composition cannot silently substitute a test entropy source.

This hardening does not claim that an OS entropy adapter is implemented in the current package. That remains a separate bounded platform-adapter stage.