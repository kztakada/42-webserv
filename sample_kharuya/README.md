
# sample_kharuya

このディレクトリ配下には、`./webserv` を実際に起動して疎通確認する E2E（エンドツーエンド）テスト群があります。

## 実行スクリプト

- `run_all_tests.sh`
	- `sample_kharuya/expect_success` / `sample_kharuya/expect_failure` / `sample_kharuya/rfc` 配下の各テストディレクトリを走査して実行します。
	- 各テストディレクトリは「`.cpp を 1 つだけ含む」想定です。
	- その `.cpp` は、同一ディレクトリの `webserv.conf` を使って `../..` の `./webserv` を起動し、ポートに接続して raw HTTP を送信し、ステータスコード等を検証します。

## 前提条件

- **Linux 環境が前提**（デフォルトでは macOS だと実行を拒否します）
	- 理由: `./webserv` が epoll 前提のため。
	- OrbStack の Ubuntu 等、Linux 側で実行してください。
- `make` と `c++`（C++98 コンパイラ）が利用できること
- `timeout` コマンドは **あれば使用**されます（無い場合はタイムアウトなしで実行）

## 使い方

リポジトリルートから:

```bash
bash sample_kharuya/run_all_tests.sh
```

スクリプトは `./webserv` が無い場合 `make` を実行してビルドします。
また、OS とバイナリ形式が不一致っぽい場合（例: Linux で Mach-O / macOS で ELF）には `make re` でリビルドを試みます。

## オプション

`bash sample_kharuya/run_all_tests.sh -h` でヘルプが表示されます。

- `--keep-going`
	- 失敗があっても最後まで実行を続けます（デフォルトは失敗したら停止）
- `--keep-bin`
	- 各テストディレクトリに生成される実行ファイル `.run_test` を削除しません（デフォルトは都度削除）
- `--only <cat>`
	- 1カテゴリのみ実行します: `expect_success` / `expect_failure` / `rfc`
- `--success-only` / `--failure-only` / `--rfc-only`
	- `--only` のショートカットです
- `--filter <substr>`
	- テストディレクトリのパスに `<substr>` を含むものだけ実行します
- `--timeout <sec>`
	- `timeout` が存在する場合の **1 テストあたり**の制限秒数（デフォルト: 20）
- `--allow-darwin`
	- macOS 上でも「実行を試みる」ためのフラグです（基本非推奨）

## 実行例

```bash
# すべて実行
bash sample_kharuya/run_all_tests.sh

# 失敗しても最後まで回す
bash sample_kharuya/run_all_tests.sh --keep-going

# 生成された .run_test を残す（失敗調査向け）
bash sample_kharuya/run_all_tests.sh --keep-bin

# RFC カテゴリだけ実行
bash sample_kharuya/run_all_tests.sh --rfc-only

# パスに "host" を含むテストだけ実行
bash sample_kharuya/run_all_tests.sh --filter host

# タイムアウトを 60 秒に変更（timeout がある環境で有効）
bash sample_kharuya/run_all_tests.sh --timeout 60
```

## テストの追加

以下のいずれかに「新しいディレクトリ」を作り、その中に次の 2 つを置きます。

- `expect_success/<test_name>/`
- `expect_failure/<test_name>/`
- `rfc/<test_name>/`

必要なファイル:

- `webserv.conf`
- `*.cpp`（ディレクトリ内に 1 つだけ）

`.cpp` はスクリプトから `-std=c++98 -Wall -Wextra -Werror` で単体コンパイルされ、バイナリは `.run_test` として作成されます。

## トラブルシュート

- macOS で `ERROR: This E2E runner expects a Linux environment...` が出る
	- Linux（OrbStack Ubuntu 等）で実行してください。どうしても試したい場合のみ `--allow-darwin`。
- `./webserv` が見つからない / 実行できない
	- リポジトリルートで `make` が通るか確認してください。
- `timeout` が無くてハングする
	- `timeout` を導入するか、テスト対象を `--filter` で絞って調査してください。

