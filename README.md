# webserv仕様

## http

### Content-Typeの判断
#### 静的レスポンスの場合
1. 拡張子による判断
2. 拡張子がない、または不明な場合はtext/plainとして扱う
#### CGIレスポンス
1. 基本的にはCGIからのレスポンスヘッダーの記述を採用
2. ヘッダーに記述がない、または不明な場合はtext/plainとして扱う
[CGIプログラムが Location: https://example.com のようなリダイレクトヘッダーだけを返す場合は、Content-Type が不要]

#### セキュリティー対策
- デフォルトでMIMEスニッフィングを返す
```
X-Content-Type-Options: nosniff
```
