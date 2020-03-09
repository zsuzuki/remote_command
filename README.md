# remote_command
 A test to execute a command remotely.

リモートからコマンドを呼び出すテスト。
セキュリティ的に何も考えていない。

## ビルド

```shell
> mkdir build
> cd build
> cmake -G Ninja ..
> ninja
```
### 環境
- c++14
- boost 1.64 以上

## 実行
### サーバ
```shell
> ./build/serv
```
### クライアント
```shell
> ./build/cli localhost ls
total 24
-rw-r--r--   1 user  staff   825  3  7 10:42 CMakeLists.txt
-rw-r--r--   1 user  staff  1073  3  6 23:39 LICENSE
-rw-r--r--   1 user  staff   248  3  6 23:48 README.md
drwxr-xr-x  11 user  staff   352  3  8 17:21 build
drwxr-xr-x   3 user  staff    96  3  6 23:42 client
drwxr-xr-x   3 user  staff    96  3  8 17:21 include
drwxr-xr-x   3 user  staff    96  3  6 23:42 server
no error
Finished
```
### 結果(サーバ)
```shell
> ./build/serv
CMD: ls -l ..
stdout: total 24
stdout: -rw-r--r--   1 user  staff   825  3  7 10:42 CMakeLists.txt
stdout: -rw-r--r--   1 user  staff  1073  3  6 23:39 LICENSE
stdout: -rw-r--r--   1 user  staff   248  3  6 23:48 README.md
stdout: drwxr-xr-x  11 user  staff   352  3  8 17:21 build
stdout: drwxr-xr-x   3 user  staff    96  3  6 23:42 client
stdout: drwxr-xr-x   3 user  staff    96  3  8 17:21 include
stdout: drwxr-xr-x   3 user  staff    96  3  6 23:42 server
done.
```
