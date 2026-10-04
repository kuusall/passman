# Passman

Passman is a local-first password manager written in C++20 for Arch Linux and macOS. It stores one encrypted vault on disk and provides an interactive CLI
for entry management, search, password generation, clipboard copying, import,
export, and master-password rotation.

> **Status: v2.0.0 release candidate.** The supported and validated targets are
> Arch Linux and macOS. Windows is unverified.

## Features

- Argon2id key derivation through libsodium.
- Authenticated encryption with `crypto_secretbox_easy`.
- Atomic owner-only vault writes and vault format versioning.
- Add, list, get, search, delete, import, and export operations.
- Secure random password generation.
- Master-password rotation with a fresh salt and nonce.
- 30-second inactivity lock and clipboard cleanup.
- CTest coverage plus AddressSanitizer and UndefinedBehaviorSanitizer builds.

## Architecture

```text
CLI / interactive parser
          |
       Session
          |
       Commands
          |
        Vault ---- Entries
          |
        Crypto
          |
   encrypted vault.dat
```

See [docs/architecture.md](docs/architecture.md) for component ownership and
[docs/cryptography.md](docs/cryptography.md) for the cryptographic construction.

## Requirements

- Arch Linux (x86_64) or macOS (arm64) for the supported release targets.
- CMake 3.20 or newer.
- A C++20 compiler (GCC or Clang).
- libsodium (`sudo pacman -S libsodium` on Arch, `brew install libsodium` on macOS).
- Clipboard support: `wl-clipboard` on Arch Linux, native `pbcopy`/`pbpaste` on macOS.

## Build and install

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
cmake --install build-release --prefix "$HOME/.local"
```

The install target installs only the `passman` executable. 

**Tip:** Ensure your shell's `PATH` includes `~/.local/bin`. If it doesn't, add the following line to your `.zshrc` or `.bashrc`:
`export PATH="$HOME/.local/bin:$PATH"`

## Quick Start

Get value from Passman in 30 seconds:

1. **Initialize**: Run `passman`. It will prompt for a master password to create a new vault.
2. **Add Secret**: Use `add github.com` and enter your password.
3. **Retrieve**: Use `get github.com` to see your password, or `get github.com --copy` to copy it to the clipboard.

## Usage

Run `passman` from the directory containing the vault. The default vault path
is `./vault.dat`; the current CLI intentionally does not read a global
configuration file.

```text
passman --help
passman
passman generate 32
```

Interactive commands:

```text
add <site> [--generate]   get <site> [--copy]
list                      search <query>
delete <site>             change-master
generate [length]         export <path>
import <path>             help
exit
```

Exports are deliberately unencrypted and are never overwritten. Treat them as
secrets and remove them securely after use.

For a complete first-run walkthrough, command reference, backup guidance, and
error reference, see [docs/usage.md](docs/usage.md).

## Testing and development

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure

cmake -S . -B build-sanitize -DPASSMAN_ENABLE_SANITIZERS=ON
cmake --build build-sanitize
ctest --test-dir build-sanitize --output-on-failure
```

The project layout and contribution process are described in
[docs/development.md](docs/development.md) and [CONTRIBUTING.md](CONTRIBUTING.md).

## Security model and limitations

The vault provides confidentiality and tamper detection when the master
password is strong and the operating system and filesystem are trusted.
Passman cannot protect against malware, keyloggers, a compromised account,
memory inspection, an attacker who can use the unlocked process, or physical
compromise of the device. Plaintext exports and clipboard contents are outside
the encrypted-vault boundary. Read [SECURITY.md](SECURITY.md) before using
Passman with important data.

Back up the encrypted `vault.dat` using a protected backup mechanism. A backup
does not replace the master password, and old backups may remain decryptable
with an older password after rotation.

## Release

The release archive is produced for Arch Linux and macOS. Verify an archive with:

```sh
shasum -a 256 -c SHA256SUMS
```

See [CHANGELOG.md](CHANGELOG.md) for v1.0.0 changes and
[docs/vault-format.md](docs/vault-format.md) for the on-disk format.
