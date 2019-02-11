# Updater and Launcher

自動アップデータ＆ランチャー

[英語](/README.md)

## Description

いわゆる、自動アップデータ兼ランチャーです。

サーバーから、最新イメージファイル(ZIPファイル)をダウンロードし、ローカルに反映して、アプリをキックします。

サーバー側に特別な仕組みは不要です。静的なファイル配信さえ出来れば、どんなサーバーでも使えます。

## Requirement

windows 環境で動作します。動作に必要なものは特にありません。

ビルドには以下のライブラリが必要です。external\\ 以下に構築する bat を用意しています。
- boost
- zlib
- openssl
- yaml-cpp

## Usage

UpdateLauncher.yml に設定を記述し、UpdateLauncher.exe と同じ場所に置きます。

あとは、UpdateLauncher.exe を実行するだけで、必要に応じてアップデートを
行い、アプリをキックします。

アップデータは、zip ファイルです。最新版のファイル一式を一般的な zip ツールで圧縮して、サーバーに配置してください。更新に必要なファイルのみをダウンロードしますので、全ファイル一式を zip に含めてしまうことが可能です。

### UpdateLauncher.yml (example)

```
# zip を展開するディレクトリ
# - "$(ExeDir)" → UpdateLauncehr.exe のあるディレクトリに置換されます
targetDir: $(ExeDir)

# ダウンロード URL
# - ローカルに存在しないファイルや、差異のあるファイルのみダウンロードします。
remoteZip: https://user:12345678@example.com/software.zip

# 最後にキックする EXE
# - "$(ExeDir)" → UpdateLauncehr.exe のあるディレクトリに置換されます
launchExe: $(ExeDir)boot.exe
```

## Feature and Limitations

- UpdateLauncher.log というフォルダにアプリログが出力されます。

- 自分自身（UpdateLauncher.exe）のアップデートも可能です。リモートの ZIP 内に自分自身のアップデータを含めることができます。
  
- zip64 及び 暗号化 zip には未対応です。deflate 以外の圧縮形式にも未対応です。

- コマンドラインツールなのでキックするだけで使用可能です。

- サーバー側に特別な仕組みは不要です。静的なファイル配信さえ出来れば、どんなサーバーでも使えます。

- ダウンロードするのは更新に必要なファイルのみです。zip ファイルを丸ごとダウンロードする訳ではありません。

- 通信経路上は圧縮された状態のデータが流れますので、帯域を無駄遣いしません。

- ダイジェスト認証は未対応です。アクセス制限が必要な場合は、HTTPS + ベーシック認証がおすすめです。

- プロキシ環境下での動作には未対応です。

- 一部機能で windows 依存コードがあります。

## Licence

[LICENSE](/LICENSE)

