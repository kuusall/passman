# Maintainer: kuusall <kuusall@example.com>
pkgname=passman
pkgver=1.0.0
pkgrel=1
pkgdesc="A local-first password manager written in C++20"
arch=('x86_64')
url="https://github.com/kuusall/passman"
license=('MIT') # Update this if you use a different license
depends=('libsodium' 'wl-clipboard')
makedepends=('cmake' 'gcc')
source=("git+https://github.com/kuusall/passman.git")
sha256sums=('SKIP')

build() {
  cmake -S "$srcdir/$pkgname" -B "$srcdir/$pkgname/build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="/usr"
  cmake --build "$srcdir/$pkgname/build"
}

package() {
  DESTDIR="$pkgdir" cmake --install "$srcdir/$pkgname/build"
}
