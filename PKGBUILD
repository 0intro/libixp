
pkgname="libixp-hg"
pkgver=131
pkgrel=1
pkgdesc="The latest hg pull of libixp, a simple 9P filesystem library"
url="http://libs.suckless.org/libixp"
license=("MIT")
arch=("i686" "x86_64")
makedepends=("mercurial")
provides=("libixp")
conflicts=("libixp")
source=()
options=(!strip)

FORCE_VER=$(hg log -r . --template {rev})

build()
{
    cd $startdir
    flags=(PREFIX=/usr \
           ETC=/etc    \
           DESTDIR="$pkgdir")

    make "${flags[@]}" || return 1
    make "${flags[@]}" install || return 1

    install -m644 -D ./LICENSE $pkgdir/usr/share/licenses/$pkgname/LICENSE
}

