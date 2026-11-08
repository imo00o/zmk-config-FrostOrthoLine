# zmk-config-FrostOrthoLine

FrostOrthoLine（12列×3行の一体型キーボード、高分解能ダイヤル + PAW3222 搭載）用の ZMK ファームウェア設定です。

- ZMK: [cormoran/zmk](https://github.com/cormoran/zmk) の `main+dya`（ZMK main / Zephyr 4.1 ベース、DYA Studio 対応）
- MCU: Seeed Studio XIAO nRF52840 Plus（ビルドボードは `xiao_ble//zmk`）
- キーマトリクス: 6行×6列（col2row）+ ダイヤルのプッシュスイッチ
- FPC1: ダイヤルモジュール（PAT9125 + プッシュスイッチ、I2C、[zmk-driver-hires-dial](https://github.com/sekigon-gonnoc/zmk-driver-hires-dial)）
  - 回転: 通常は縦スクロール、レイヤー1で音量、レイヤー7で横スクロール
  - プッシュスイッチ: 37番目のキー。通常は中クリック、レイヤー1ではミュート
- FPC2: PAW3222（SPI）
  - カーソル移動 + 自動マウスレイヤー（レイヤー6）、レイヤー5で縦スクロール、レイヤー7で横スクロール

## ビルド

`config/`・`boards/` などを変更して push すると GitHub Actions でビルドされ、Artifacts に `FrostOrthoLine` と `settings_reset` のファームウェアが出力されます。

## ファイル構成

- `boards/shields/FrostOrthoLine/FrostOrthoLine.dtsi`: キーマトリクス・プッシュスイッチ・PAW3222・FPC1 の I2C バス
- `boards/shields/FrostOrthoLine/FrostOrthoLine.overlay`: FPC1 のダイヤル（hires-dial）
- `boards/shields/FrostOrthoLine/FrostOrthoLine.conf`: ダイヤル用の Kconfig
- `config/FrostOrthoLine.conf` / `config/FrostOrthoLine.keymap`: 共通の設定とキーマップ

## DYA Studio

BLE 管理・アイドル/スリープ設定・ポインティング設定（runtime input processor）に加えて、コンボを DYA Studio から追加・編集できます（[zmk-feature-runtime-combo](https://github.com/cormoran/zmk-feature-runtime-combo)）。

編集するときは、左上のキー（Tab）を押しながら、下段の左から7番目のキーを押してロックを解除します。

## 注意点

FPC1 の基板側ピン名とダイヤルモジュールのピンは役割が一致しません。ダイヤルモジュールをつなぐと、D10（基板ピン名 RDY/CS 1）が PAT9125 の MOTION、D19（基板ピン名 GND/MOTION 1）がプッシュスイッチ、D18 が SDA、D9 が SCL になります。

ROW1 が D16（P0.31）に配線されていますが、このピンは XIAO 内蔵のバッテリー電圧測定用 ADC と同じピンです。そのためバッテリー残量の通知は無効にしています。

キーの物理配置と電気マトリクスの対応は、1本の COL が物理2列分を担当する配線です。キーボードを上から見て左から、上段が「COL5/ROW3、COL5/ROW0、COL4/ROW3、COL4/ROW0…COL0/ROW0」、中段が「COL5/ROW4、COL5/ROW1、COL4/ROW4…COL0/ROW1」、下段が「COL5/ROW5、COL5/ROW2、COL4/ROW5…COL0/ROW2」の順です（右端が COL0）。対応は `FrostOrthoLine.dtsi` の `default_transform` で定義しています。

PAW3222 の移動方向は `x-invert` / `y-invert`、ダイヤルの回転方向は `FrostOrthoLine.overlay` の `invert` で調整します。ポインティングの設定は DYA Studio からも変更できます。

ZMK 本体のヘッダは ZMK アプリにしか公開されていないため、hires-dial ドライバのビルド用にリポジトリ直下の `CMakeLists.txt` で ZMK のヘッダを全体に公開しています。

Zephyr 標準の PAT912x 入力ドライバで PAT9125 をトラックボールとして使う試験版と、そのために作ったドライバは、`hires-dial` ブランチの履歴（コミット c951e5d）に残っています。
