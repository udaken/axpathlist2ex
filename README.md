# axpathlist2ex
[axpathlist2.spi](http://artisticimitation.web.fc2.com/adbtest/) clone.

- [説明](#%E8%AA%AC%E6%98%8E)
- [要求システム](#%E8%A6%81%E6%B1%82%E3%82%B7%E3%82%B9%E3%83%86%E3%83%A0)
- [インストール](#%E3%82%A4%E3%83%B3%E3%82%B9%E3%83%88%E3%83%BC%E3%83%AB)
- [ライセンス](#%E3%83%A9%E3%82%A4%E3%82%BB%E3%83%B3%E3%82%B9)
- [仕様](#%E4%BB%95%E6%A7%98)
    - [改行コード](#%E6%94%B9%E8%A1%8C%E3%82%B3%E3%83%BC%E3%83%89)
    - [文字コード](#%E6%96%87%E5%AD%97%E3%82%B3%E3%83%BC%E3%83%89)
    - [ファイルフォーマット](#%E3%83%95%E3%82%A1%E3%82%A4%E3%83%AB%E3%83%95%E3%82%A9%E3%83%BC%E3%83%9E%E3%83%83%E3%83%88)
    - [元ファイルのファイル名を使用する](#%E5%85%83%E3%83%95%E3%82%A1%E3%82%A4%E3%83%AB%E3%81%AE%E3%83%95%E3%82%A1%E3%82%A4%E3%83%AB%E5%90%8D%E3%82%92%E4%BD%BF%E7%94%A8%E3%81%99%E3%82%8B)
    - [Susie Plug-in API](#susie-plug-in-api)
    - [INIファイル](#ini%E3%83%95%E3%82%A1%E3%82%A4%E3%83%AB)

## 説明
パスリスト(テキストファイル)を書庫として扱うSusieプラグインです。<br>
axpathlist2 の基本機能に加えて、いくつかの追加機能があります。

* Unicodeサポート(UTF-8 BOM, UTF-16 BOM)
* ワイルドカードのサポート
* 書庫内ディレクトリのサポート([iniファイル](#ini%E3%83%95%E3%82%A1%E3%82%A4%E3%83%AB)の編集が必要です)
* sz7以外の拡張子のサポート([iniファイル](#ini%E3%83%95%E3%82%A1%E3%82%A4%E3%83%AB)の編集が必要です)
* パフォーマンスの改善(環境、使用方法によります)

## 要求システム
Windows 7以降

## インストール
https://github.com/udaken/axpathlist2ex/releases からダウンロードしてください。
プラグインフォルダにコピーしてください。<br>
アンインストールは、レジストリは使用しないのでファイルを削除してください。

## ライセンス
MIT License

このソフトウェアの使用は全て自己責任でお願いします。 

## 仕様
### 改行コード
下記をサポートしますが、CRLF推奨です。
* CRLF
* LF

### 文字コード
下記をサポートします。(自動判定)
* UTF-8 BOM
* UTF-16 Little Endian BOM
* CP932(Shift-JIS)

### ファイルフォーマット
ワイルドカードは、 `*` と `?` のみです。[FindFirstFile()](https://msdn.microsoft.com/ja-jp/library/cc429233.aspx)の仕様に従います。<br>
相対パスはサポートしていません。

### 元ファイルのファイル名を使用する
axpathlist2 では元のファイル名に関係なくファイル名が連番になりますが、
INIファイルを編集すると、「連番のフォルダ\元ファイル名」という形式の書庫にできます。
```
000000001\Original1.jpg
000000002\Original2.bmp
```

### Susie Plug-in API
ファイル入力のみです。

### INIファイル
axpathlist2ex.spiと同じ場所に `axpathlist2ex.ini` を作成してください。
```
[AXPATHLIST2EX]
EXTENSION=*.foo ; 対応する拡張子
USE_FILENAME=1 ;元ファイルのファイル名を使用する場合は、0以外の数値を指定してください。
OVERRIDE_CODEPAGE=932 ; 誤判定が起きる場合、コードページを指定してください。強制的にShift-JISにする場合は、932と指定してください。
```

