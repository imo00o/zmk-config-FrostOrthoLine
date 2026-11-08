# zmk-driver-hires-dial は behavior を独自の zephyr_library でビルドするが、
# ZMK v0.3 は app/include を app ターゲットにしか公開しないため
# <drivers/behavior.h> などが見つからない。ZMK のヘッダを全ライブラリに見えるようにする。
zephyr_include_directories(${APPLICATION_SOURCE_DIR}/include)
