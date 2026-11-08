# zmk-driver-hires-dial は behavior を独自の zephyr_library でビルドするが、
# ZMK は app/include を app ターゲットにしか公開しないため
# <drivers/behavior.h> などが見つからない。ZMK のヘッダを全ライブラリに見えるようにする。
zephyr_include_directories(${APPLICATION_SOURCE_DIR}/include)

# PAT9125 を X/Y カーソル入力として使うドライバ（FrostOrthoLine_pat9125 シールドで使用）
if(CONFIG_FROSTORTHOLINE_PAT9125)
  zephyr_library()
  zephyr_library_sources(src/pat9125_input.c)
endif()
