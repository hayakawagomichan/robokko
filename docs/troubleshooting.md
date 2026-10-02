# 書き込みで困ったとき

[← 書き込み手順](../README.md#はじめに)

## Uploadボタンが見つからない

左側のPlatformIOアイコン → **PROJECT TASKS → m5stack-atoms3r → General → Upload** を選びます。
項目が出ない場合は、拡張機能の準備が終わったか、`platformio.ini` が入ったフォルダを開いているかを確認してください。

## 顔やサーボが動かない

| 症状 | 確認すること |
| --- | --- |
| 画面が消えた | USBの接続。バッテリー使用時はスイッチと充電残量 |
| SUCCESSなのに顔が出ない | USBを抜き差しする。画面付きATOM S3Rか確認 |
| 顔は出るがサーボが動かない | 電源を切り、[GND・5V・G2の配線](../README.md#servo-wiring)を確認 |
| 引っかかる・うなる・再起動する | すぐ電源を切り、取り付け・配線・機構の引っかかりを確認 |

### ビルド用ファイルが見つからないとき

`bootloader_qio_80m.elf` や `freertos/FreeRTOS.h` が見つからず、エラーの場所が `.platformio/packages/framework-arduinoespressif32/` の中を指している場合は、PCに入ったビルド用パッケージが欠けている可能性があります。
これはGitHubから受け取るロボっこのファイルとは別に、PlatformIOが準備するものです。

まず、[フォルダの置き場所](../README.md#download)の英数字の場所を使っていることと、初回のダウンロードが終わったことを確認します。それでも同じ場合は、次の方法で該当パッケージを入れ直せます。**正常にビルドできる方は実行不要です。**

1. 実行中のBuild・Uploadを止め、インターネットにつなぎます。
2. VS Codeのコマンドパレット（Windowsは `Ctrl + Shift + P`、Macは `⌘ + Shift + P`）で **`PlatformIO: Open PlatformIO Core CLI`** を選びます。
3. 開いたターミナルで、次の1行を実行します。このプロジェクトが使用しているバージョンのビルド用パッケージを再インストールします。同じパッケージを使う他のPlatformIOプロジェクトにも共通の環境です。

```sh
pio pkg install --global --tool "platformio/framework-arduinoespressif32@3.20016.0" --force
```

4. 再インストールが完了したら「✓ Build」で確認し、成功してから「→ Upload」を押します。

解決しない場合は、エラーの全文を[Issues](https://github.com/hayakawagomichan/robokko/issues)へお寄せください。ユーザー名などの個人情報は伏せて構いません。
[PlatformIO公式：パッケージの再インストール](https://docs.platformio.org/en/stable/core/userguide/pkg/cmd_install.html)

### 書き込み先が見つからないとき

`Please specify` と `upload_port` を含むエラーや、その次の `For some development platforms it can be a USB flash drive` という表示は、書き込み先を自動検出できなかったときに出ます。ATOM S3RでUSBメモリーを用意するという意味ではありません。

1. ATOM S3Rを**データ通信対応のUSBケーブルでPCへ**つなぎます。画面がついていても、充電専用ケーブルでは書き込めません。
2. いったん抜き差しし、可能ならPCの別のUSB端子や、通信できることがわかっているケーブルを試します。
3. 下の方法で本体を**書き込みモード**にして、もう一度Uploadを押します。
4. Windowsの「デバイス マネージャー」→「ポート（COMとLPT）」で本体のCOM番号が表示されるのに失敗する場合は、下の「COM番号」の手順で接続先を指定します。番号が出ない場合は、ケーブルとUSB接続を確認してください。

先にBuildを実行しても、接続先が見つからない状態ではUploadは成功しません。エラーを相談するときは、末尾の `FAILED` だけでなく、**少し上の `Error:` から始まる行**も見せてください。
[PlatformIO公式：接続先の自動検出とupload_port](https://docs.platformio.org/en/latest/projectconf/sections/env/options/upload/upload_port.html)

### 書き込みモードにする方法

本体の **リセットボタン** を約2秒長押しし、内部の緑LEDが点灯したら離します。
画面部分の操作ボタンとは別のボタンです。その後、もう一度 **Upload** を押してください。
[M5Stack公式の説明と写真](https://docs.m5stack.com/en/core/AtomS3R)

<details>
<summary>接続先の「COM番号」を指定したいとき（Windows）</summary>

1. Windowsのスタートボタンを右クリックして「デバイス マネージャー」を開きます。
2. 「ポート（COMとLPT）」を開き、ATOM S3Rをつないだときに増える項目の番号を確認します。例：`COM4`。
3. 自動検出でうまくいかない場合は、`platformio.ini` の末尾に `upload_port = COM4` を追加して保存し、Uploadします。番号は自分のPCで確認したものに置き換えてください。

書き込みモードにしたり、USBの接続先を変えたりすると番号が変わる場合があります。
別のPCへ渡すときは、追加した `upload_port` の行を削除して自動検出に戻してください。

</details>
