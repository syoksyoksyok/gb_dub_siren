# Syok Dub Siren GB

Version: 1.0.0

GBDK-2020 で作成した DMG Game Boy 向けダブサイレン homebrew ROM です。

このプロジェクトは非公式の自作ソフトウェアであり、Nintendo Co., Ltd. とは関係ありません。Game Boy は Nintendo の商標です。

## Build

GBDK-2020 をインストールしてください。Makefile は既定で `GBDK_HOME=C:/Dev_tools/gbdk` を使います。

```sh
make
```

GBDK を別の場所にインストールしている場合は `GBDK_HOME` を指定します。

```sh
make GBDK_HOME=/path/to/gbdk
```

PATH 上の `lcc` を使う場合は、変数を空にして実行します。

```sh
make GBDK_HOME=
```

ビルド時に `patch-rom-header.ps1` を実行するため、PowerShell が必要です。

出力 ROM:

```text
build/syok-dub-siren-gb.gb
```

ROMヘッダタイトルは、Game Boyヘッダの16文字制限に合わせて `SYOKDUBSIRENGB` に設定しています。

## 操作対応表

方向キーとボタンの組み合わせで各パラメータを直接変更します。上下キーでLFO波形を変更し、A + Left / Right でPITCH、B + Left / Right でRATE、Select / Start でDEPTHを変更します。

| 元の操作子 | Game Boy割り当て | 実装挙動 |
|---|---|---|
| ゲート | A | 押している間だけ発音。A + Left / Right で発音しながら PITCH 調整 |
| キル | なし | Game Boy版では未使用 |
| LFO一時停止 | なし | Game Boy版では未使用 |
| 波形切替 | Up / Down | 上下キーでLFO波形切替 |
| POT1 ピッチ | A + Left / Right | ベースピッチを増減 |
| POT2 LFO深さ | Select、Start | Selectで減少、Startで増加 |
| POT3 LFO速度 | B + Left / Right、またはA + B + Left / Right | LFO RATEを増減 |

上下キーで LFO 波形を変更できます。A ボタンは Left / Right と組み合わせると発音しながら PITCH を直接変更できます。B ボタンは Left / Right と組み合わせると LFO RATE を直接変更できます。A+B+Left/Right では発音しながら LFO RATE を変更できます。Start+Select 同時押しでHELPを表示し、Select単押しで LFO DEPTH を下げ、Start単押しで LFO DEPTH を上げます。LFO WAVE 欄には背景タイルで描いた160x48px相当の波形を表示し、現在位置をスプライトマーカーで示します。

## 仕様メモ

- Game Boy APU のチャンネル1を直接操作します。
- `NR10`-`NR14` と `NR50`-`NR52` を使用します。
- 周波数は `131072 / (2048 - freq)` Hz の関係から 11bit レジスタ値へ変換します。
- LFO は固定テーブル化した 256 ステップ値を使い、SINE / SQUARE / SAW / REV SAW を実装しています。SINE / SAW / REV SAW は16bit位相の補間で滑らかにしています。
- 発音中は NR12 の上位ニブルを最大値に設定します。
- B ボタンによるキル機能は削除しています。

## 性能改善メモ

| 対象 | 変更前 | 変更後 | 効果 |
|---|---|---|---|
| LFO値 | フレームごとに波形関数で計算 | 4波形x256ステップの固定テーブルを参照 | 分岐と計算を削減 |
| LFO位相 | 8bit位相 | 16bit位相 + 補間 | ピッチ変調を滑らかにする |
| CH1周波数更新 | 毎フレームCH1を再トリガ | 発音開始時だけトリガし、LFO中は周波数だけ更新 | 音の粗さと再トリガ負荷を削減 |
| LFO波形表示 | UI更新ごとに文字で波形を再描画 | 固定Y座標テーブルをDEPTHに応じてスケールし、背景タイルで160x48px相当の波形を生成 | 通常フレームのCPU負荷を削減し、表示を滑らかにする |
| 波形表示解像度 | 文字ベース表示 | 20x6背景タイル内のピクセル線 | 横160px/縦48px相当で細かく表示 |
| UI数値表示 | 各パラメータに数値行あり | つまみ付きスライダー表示中心に整理 | 波形表示領域を確保し、描画文字数を抑制 |
| ROM/RAM使用 | テーブルなし | 波形表示用タイルバッファとY座標バッファを追加 | RAM増加より描画負荷削減を優先 |

## 既知の制限

- 発音中のCH1音量は最大15/15、マスター音量は最大7/7に設定しています。
- 元のアナログポットと異なり、パラメータは方向キーで段階的に変更します。起動時はSQUARE波形、PITCHは440Hz、DEPTHは400Hz、LFO RATEは 1〜80 の 19 です。LFO RATE の範囲は約0.159Hz〜約12.73Hzです。Start+Select 同時押しでHELPを表示できます。
- LFO 波形データは 256 ステップのルックアップテーブルとして保持しています。
- 実機/エミュレータの APU 差により、音量フェードや再トリガの聴こえ方が多少変わります。

## License

MIT License. See [LICENSE](LICENSE).
