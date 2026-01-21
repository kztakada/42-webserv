#ifndef UTILS_RESULT_HPP_
#define UTILS_RESULT_HPP_

#include <cassert>
#include <string>

/* Usage of Result class

  ===== Overview =====
  C++98 で Rust の Result 型のような機能を実装する｡
  エラーが発生するかもしれない関数の返り値として利用する｡

  ===== Usage =====
  Result<int> succeed() {  // 成功
    return 10; // 暗黙の型変換が働く （int -> Result<int>）
  }

  Result<void> succeedVoid() {  // 成功 (返り値がvoid)
    return Result<void>();
  }

  Result<std::string> fail() {  // エラー
    return Result<std::string>(ERROR, "This is error!");
  }

  Result<int> failSimple() {  // エラー（メッセージなし）
    return Result<int>(ERROR);
  }

  int main(){
    Result<int> a = succeed();
    if (a.isOk()) {
      std::cout << "val: " << a.unwrap() << std::endl;
    } else {
      std::cout << "err: " << a.getErrorMessage() << std::endl;
    }
  }
*/
namespace utils
{
namespace result
{

enum ResultType  // Resultステータスの定義
{
    OK,
    ERROR
};

// ===== Resultの基底クラス。共通のインターフェースと実装を提供する =====
class ResultBase
{
   public:
    virtual ~ResultBase() {}

    // 共通メソッド
    bool isOk() const { return status_ == OK; }  // 成功状態かどうかを返す
    bool isError() const
    {
        return status_ == ERROR;
    }  // エラー状態かどうかを返す
    std::string getErrorMessage() const  // エラーメッセージを取得
    {
        assert(status_ == ERROR);  // エラー状態でない場合は assert で落ちる
        return err_msg_;
    }

   protected:
    // 共通メンバ変数
    ResultType status_;    // 成功/失敗の状態管理
    std::string err_msg_;  // エラーメッセージ

    // 基底クラス初期化コンストラクタ
    ResultBase() : status_(OK), err_msg_() {}  // 成功状態で初期化
    explicit ResultBase(const std::string& err_msg)
        : status_(ERROR), err_msg_(err_msg)
    {
    }  // エラー状態で初期化
};

// ===== 返り値が通常の型の場合の Result クラス =====
template <typename T>
class Result : public ResultBase
{
   private:
    // メンバ変数
    T val_;

   public:
    // 初期化コンストラクタ
    Result(const T& val) : ResultBase(), val_(val) {}  // 成功状態で初期化
    explicit Result(  // エラー状態で初期化（エラーメッセージはオプション）
        ResultType type, const std::string& err_msg = "")
        : ResultBase(err_msg), val_()
    {
        (void)type;
        assert(type == ERROR);
    }
    explicit Result(  // T
                      // がデフォルトコンストラクタを持たない場合用（ダミー値でエラー初期化）
        ResultType type, const T& dummy_val, const std::string& err_msg = "")
        : ResultBase(err_msg), val_(dummy_val)
    {
        (void)type;
        assert(type == ERROR);
    }

    // メソッド
    T unwrap()  // 成功した値を取得
    {
        assert(isOk());  // エラー状態の場合は assert で落ちる
        return val_;
    }
};

// ===== 返り値が参照の場合の Result クラス =====
template <typename T>
class Result<T&> : public ResultBase
{
   public:
    typedef T ValueType;  // 参照の元の型を抽出
    typedef ValueType& Reference;

   private:
    ValueType tmp_lval_;  // エラーで初期化する場合の参照先として使う一時変数
    Reference val_;

   public:
    // 初期化コンストラクタ
    Result(Reference val) : ResultBase(), val_(val) {}  // 成功状態で初期化
    explicit Result(  // エラー状態で初期化（エラーメッセージはオプション）
        ResultType type, const std::string& err_msg = "")
        : ResultBase(err_msg), tmp_lval_(), val_(tmp_lval_)
    {
        (void)type;
        assert(type == ERROR);
    }
    explicit Result(  // T
                      // がデフォルトコンストラクタを持たない場合用（ダミー値でエラー初期化）
        ResultType type, Reference dummy_val, const std::string& err_msg = "")
        : ResultBase(err_msg), tmp_lval_(dummy_val), val_(tmp_lval_)
    {
        (void)type;
        assert(type == ERROR);
    }

    // メソッド
    Reference unwrap()  // 成功した値を取得
    {
        assert(isOk());  // エラー状態の場合は assert で落ちる
        return val_;
    }
};

// ===== 返り値が void の場合の Result クラス =====
template <>
class Result<void> : public ResultBase
{
   public:
    // 初期化コンストラクタ
    Result() : ResultBase() {}  // 成功状態で初期化
    explicit Result(  // エラー状態で初期化（エラーメッセージはオプション）
        ResultType type, const std::string& err_msg = "")
        : ResultBase(err_msg)
    {
        (void)type;
        assert(type == ERROR);
    }

    // メソッド
    void unwrap() const  // 成功した値を取得
    {
        assert(isOk());  // エラー状態の場合は assert で落ちる
    }
};

}  // namespace result
}  // namespace utils
#endif
