# zmk-config-FrostOrthoLine（hires-dial ブランチ）

FrostOrthoLine（12列×3行・PAW3222 + PAT9125 搭載の一体型キーボード）用の ZMK ファームウェア設定です。

- ZMK: [cormoran/zmk](https://github.com/cormoran/zmk) の `main+dya`（ZMK main / Zephyr 4.1 ベース、DYA Studio 対応）
- MCU: Seeed Studio XIAO nRF52840 Plus（ビルドボードは `xiao_ble//zmk`）
- キーマトリクス: 6行×6列（col2row）
- PAW3222（FPC1, SPI）: カーソル移動 + 自動マウスレイヤー（レイヤー6）、レイヤー5で縦スクロール、レイヤー7で横スクロール
- PAT9125（FPC2, I2C）: シールドによって使い方が変わります

PAW3222 を2個使う構成は `main` ブランチにあります（こちらは ZMK v0.3 のままです）。

## ビルドされるファームウェア

- `FrostOrthoLine` のファームウェア: FPC2 の PAT9125 を高分解能ダイヤル（[zmk-driver-hires-dial](https://github.com/sekigon-gonnoc/zmk-driver-hires-dial)）として使います。通常は縦スクロール、レイヤー1で音量、レイヤー7で横スクロールです。
- `FrostOrthoLine_pat9125` のファームウェア: トラックボールの試験用です。Zephyr 標準の `pixart,pat912x` ドライバで、PAT9125 の X/Y をカーソル移動として出します。
- `settings_reset` のファームウェア: 設定リセット用です。

`config/`・`boards/` などを変更して push すると GitHub Actions でビルドされ、Artifacts に出力されます。

## ファイル構成

- `boards/shields/FrostOrthoLine/FrostOrthoLine.dtsi`: 共通部分（キーマトリクス・PAW3222・FPC2 の I2C バス）
- `boards/shields/FrostOrthoLine/FrostOrthoLine.overlay`: ダイヤル版
- `boards/shields/FrostOrthoLine/FrostOrthoLine_pat9125.overlay`: PAT9125 トラックボール試験版
- `config/FrostOrthoLine.conf` / `config/FrostOrthoLine.keymap`: 両シールド共通の設定とキーマップ

## DYA Studio

BLE 管理・アイドル/スリープ設定・ポインティング設定（runtime input processor）に加えて、コンボを DYA Studio から追加・編集できます（[zmk-feature-runtime-combo](https://github.com/cormoran/zmk-feature-runtime-combo)）。

## 注意点

FPC2 の基板側ピン名とダイヤルモジュールのピンは役割が一致しません。ダイヤルモジュールをつないだときは、D12（基板ピン名 RDY/CS）が PAT9125 の MOTION、D1（基板ピン名 GND/MOTION）がプッシュスイッチになります。プッシュスイッチは37番目のキーとして扱い、通常は中クリック、レイヤー1ではミュートを割り当てています。

ROW1 が D16（P0.31）に配線されていますが、このピンは XIAO 内蔵のバッテリー電圧測定用 ADC と同じピンです。そのためバッテリー残量の通知は無効にしています。

キーの物理配置と電気マトリクスの対応は「左6列 = ROW0〜2、右6列 = ROW3〜5」を想定しています。実際の配線と違う場合は `FrostOrthoLine.dtsi` の `default_transform` を修正してください。

センサーの移動方向は `x-invert` / `y-invert`（PAT9125 トラックボール版は `invert-x` / `invert-y` またはランタイムプロセッサ `pat9125`）で調整します。DYA Studio からも変更できます。

ZMK v0.3 系・main 系とも、ZMK 本体のヘッダは ZMK アプリにしか公開されていないため、hires-dial ドライバのビルド用にリポジトリ直下の `CMakeLists.txt` で ZMK のヘッダを全体に公開しています。
