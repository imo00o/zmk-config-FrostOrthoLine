# zmk-config-FrostOrthoLine

FrostOrthoLine（12列×3行・PAW3222 ×2 搭載の一体型キーボード）用の ZMK ファームウェア設定です。

- MCU: Seeed Studio XIAO nRF52840 Plus（ビルドボードは `seeeduino_xiao_ble`）
- キーマトリクス: 6行×6列（col2row）
- PAW3222 #1（FPC1）: カーソル移動 + 自動マウスレイヤー（レイヤー6）、レイヤー5で縦スクロール、レイヤー7で横スクロール
- PAW3222 #2（FPC2）: 常時スクロール

## ビルド

`config/` を変更して push すると GitHub Actions でビルドされ、Artifacts に `FrostOrthoLine-seeeduino_xiao_ble-zmk.uf2` と `settings_reset-seeeduino_xiao_ble-zmk.uf2` が出力されます。

## 注意点

ROW1 が D16（P0.31）に配線されていますが、このピンは XIAO 内蔵のバッテリー電圧測定用 ADC と同じピンです。そのためバッテリー残量の通知は無効にしています。

キーの物理配置と電気マトリクスの対応は「左6列 = ROW0〜2、右6列 = ROW3〜5」を想定しています。実際の配線と違う場合は `FrostOrthoLine.overlay` の `default_transform` を修正してください。

センサーの移動方向は `x-invert` / `y-invert` で調整します。DYA Studio からも変更できます。
