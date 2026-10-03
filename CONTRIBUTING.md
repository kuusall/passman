# Contributing

Passman is a security-sensitive C++ project. Open a focused branch and pull
request for each change. Explain behavior changes and include regression tests
where practical.

Before submitting:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Use C++20, existing warning flags, RAII, explicit bounds checks, and the
existing libsodium APIs. Security-sensitive changes should update the relevant
documentation and explain key, nonce, memory, and error-path behavior.

### Pull Request Checklist

Before submitting your PR, please ensure:
- [ ] **Builds clean**: The project builds without warnings on all supported platforms.
- [ ] **Tests pass**: All `ctest` cases pass, including those in the `build-sanitize` configuration.
- [ ] **Documentation**: Any changes to features, vault format, or API are reflected in the `docs/` directory.
- [ ] **Security**: No sensitive data (passwords, tokens, personal paths) have been committed.
- [ ] **Style**: Code follows the existing project style and RAII patterns.

Never commit passwords, vault files, plaintext exports, private keys, tokens,
personal paths, or other private data. Do not weaken authentication or disable
sanitizers to make a test pass. Review [SECURITY.md](SECURITY.md) before
reporting a vulnerability; do not disclose an exploitable issue in a public
issue.
