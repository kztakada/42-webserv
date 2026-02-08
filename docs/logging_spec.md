# 処理負荷計測ログのファイル出力

## 仕様
- 現在staticクラスとしてしか使用していないLogクラスに、インスタンス化実装を追加することで実現する
- Serverクラスは自身の初期化時に、Logクラスのインスタンスを１つだけ所有するようにする
- Logインスタンスはis_runningをプライベートプロパティとして持っており、初期化時はfalseとする
- Logインスタンスはis_runningがtrueになると、保持しているキャッシュログを1秒毎にlogs/processing.txt に追記して、キャッシュをクリアする
- ServerクラスはLogインスタンスの参照をログ出力箇所まで渡して、それを使用させることで、シングルトンのログシステムを実現する
- Serverクラスはstart()メソッド内のwhileループに入る前にLogインスタンスをrun()させて、whileループを抜ける時にstop()させる
- LogクラスでTimestampを発行する場合、utils::Timestampクラスを利用するようにしてください。（std::timeを使用する必要があれば、utils::Timestampクラスを拡張して使用するようにしてください）

## 実装指示
上記仕様を踏まえた上で、以下の計測項目に対して、
- Serverクラスから渡されたLogインスタンスに、計測内容を渡すように実装する


## 計測項目
1. 同時接続コネクション数 (Active Connections) [ActiveConn]
• 計測内容: クライアント接続中のアクティブなTCPコネクション数。
• 計測方法: 計測時点での更新された最新のコネクション数を採用する。
ActiveConn: Listener→HttpSession delegate成功時に増、HttpSession破棄で減（Keep-Aliveは接続維持なので減らない）

2. イベントループの周回時間 (Event Loop Latency) [LoopTime(s)]
• 計測内容: 1回の while ループ（waitEvents から戻ってきて、全てのイベントを処理して、また waitEvents に入るまでの時間）。ループの開始時刻と終了時刻の差分によって算出。
• 計測方法: 同じ期間に複数候補がある場合は最大時間を採用する
LoopTime(s): waitEvents復帰後〜dispatch/timeout処理完了までの最大値（秒）

3. リクエスト処理時間 (Request Processing Time) [ReqTime(s)]
クライアントからのリクエストを受信し始めてから、レスポンスを返し終えるまでのトータル時間です。
• 計測内容:
    1. accept または最初の recv 時刻
    2. レスポンス送信完了 (send 完了) 時刻
    ◦ この2点の差分。
• 計測方法: 同じ期間に複数候補がある場合は最大時間を採用する。
ReqTime(s): 最初のrecv時刻を記録し、レスポンス送信完了時に差分を報告（期間内最大）


4. CGI同時実行数 [CGI_Count]
• 計測内容: 現在実行中のCGIプロセス（子プロセス）の数。
• 計測方法: 計測時点での更新された最新のコネクション数を採用する。
CGI_Count: CGI session delegate成功時に増、破棄で減


5. I/O待機発生回数 (EAGAIN/EWOULDBLOCK) [Block_I/O]
• 計測内容: read/recv や write/send を呼んだ際に、データがまだなくて（あるいはバッファがいっぱいで）処理できず、関数が戻ってきた回数。（ノンブロッキングI/Oが正しく機能しているかの指標になる）
• 計測方法: 期間毎の合計数をカウントする
Block_I/O: read/writeが進まない（戻り値<0で「今は進めない」扱い）回数を期間合計



## 出力サンプル
[Timestamp], [ActiveConn], [LoopTime(s)], [ReqTime(s)], [CGI_Count], [Block_I/O]
10:00:01,    50,           2,              100,       0,           0
10:00:02,    120,          15,             250,       5,           0
10:00:03,    300,          45,             400,       12,          1
...