# Security Policy

This project intentionally reaches into Node/V8 private runtime state. It is a
diagnostic and integration tool, not a sandbox boundary or a hardening layer.

## Supported Versions

Only the current `main` branch is supported before a stable 1.0 release.

## Reporting a Vulnerability

Use GitHub's private vulnerability reporting for this repository when possible.
If that is unavailable, open a minimal public issue that says a security report
is available, but do not include exploit details in the issue body.

Useful reports include:

- Node version and distribution.
- Platform, architecture, and Linux libc if relevant.
- Selected backend and ABI.
- `probe()` diagnostics with sensitive local paths removed.
- A minimal reproduction.

## Non-goals

This package does not prevent access to Node internals by code that can already
load native addons. It also does not make internal Node modules stable or safe to
use across Node releases.
