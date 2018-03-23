
pkgname="libixp"
pkgver=131
pkgrel=1
pkgdesc="a simple 9P filesystem library"
url="http://github.com/0intro/libixp"
license=(MIT)
arch=(i686 x86_64)
provides=(libixp)
conflicts=(libixp)
source=()
options=(!strip)

build()
{
    cd $startdir
    flags=(PREFIX=/usr \
           ETC=/etc    \
           DESTDIR="$pkgdir")

    make "${flags[@]}" || return 1
}
package()
{
    cd $startdir
    flags=(PREFIX=/usr \
           ETC=/etc    \
           DESTDIR="$pkgdir")

    make "${flags[@]}" install || return 1

    install -m644 -D ./LICENSE $pkgdir/usr/share/licenses/$pkgname/LICENSE
}
