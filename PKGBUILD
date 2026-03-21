# Maintainer: darudas
# Maxine PipeWire - NVIDIA Broadcast for Linux

pkgname=maxine-pipewire
pkgver=0.1.0
pkgrel=1
pkgdesc='NVIDIA Maxine audio/video effects integration for PipeWire'
arch=('x86_64')
url='https://github.com/darudas/maxine-pipewire'
license=('MIT')
depends=(
    'pipewire'
    'systemd-libs'
)
makedepends=(
    'meson'
    'ninja'
    'libpipewire'
)
optdepends=(
    'nvidia-maxine-afx-sdk: NVIDIA Maxine Audio Effects SDK (required at runtime)'
    'nvidia-utils: NVIDIA driver (required at runtime)'
)
source=("${pkgname}-${pkgver}.tar.gz::${url}/archive/v${pkgver}.tar.gz")
sha256sums=('SKIP')

build() {
    cd "${pkgname}-${pkgver}"
    arch-meson build \
        -Dvideo_effects=false \
        -Dsystemd=true
    meson compile -C build
}

package() {
    cd "${pkgname}-${pkgver}"
    meson install -C build --destdir "${pkgdir}"

    install -Dm644 LICENSE "${pkgdir}/usr/share/licenses/${pkgname}/LICENSE"
    install -Dm644 config/60-maxine-echo-cancel.conf \
        "${pkgdir}/usr/share/doc/${pkgname}/60-maxine-echo-cancel.conf"
    install -Dm644 README.md "${pkgdir}/usr/share/doc/${pkgname}/README.md"
}
