#pragma once
#include <string>
using namespace std;

// TokenType 枚举：表示词法分析器输出的各种符号类型
enum class TokenType
{
    // 字面量与标识符
    ID,     // 标识符
    NUMBER, // 数字字面量

    // 关键字
    INT,      // int 关键字
    VOID,     // void 关键字
    IF,       // if 关键字
    ELSE,     // else 关键字
    WHILE,    // while 关键字
    RETURN,   // return 关键字
    BREAK,    // break 关键字
    CONTINUE, // continue 关键字

    // 算术运算符
    PLUS,  // '+'
    MINUS, // '-'
    TIMES, // '*'
    DIV,   // '/'
    MOD,   // '%'

    // 关系运算符
    GT, // '>'
    LT, // '<'
    GE, // '>='
    LE, // '<='
    EQ, // '=='
    NE, // '!='

    // 逻辑运算符
    OR,  // '||'
    AND, // '&&'
    NOT, // '!' (一元逻辑非)

    // 赋值运算符
    ASSIGN, // '='

    // 分隔符
    LPAREN, // '('
    RPAREN, // ')'
    LBRACE, // '{'
    RBRACE, // '}'
    SEMI,   // ';'
    COMMA,  // ','

    // 文件结束与未知
    END,    // 文件结束标记
    UNKNOWN // 未知或非法 Token
};

// Token 结构体：表示词法单元
struct Token
{
    TokenType type; // Token 类型
    string lexeme;  // 原始文本内容
    int line;       // 所在行号，用于错误定位
};

// 词法分析器类：负责将源代码字符串拆分成 Token 流
class Lexer
{
public:
    // 构造函数：传入整个源代码文本
    Lexer(const string &source);

    // 返回下一个 Token
    Token nextToken();

private:
    string src;     // 源代码文本
    size_t pos = 0; // 当前扫描位置
    int line = 1;   // 当前行号，从 1 开始

    // 查看当前字符但不消费
    char peek() const;

    // 消费并返回当前字符
    char advance();

    // 跳过空白字符（空格、制表符、换行）及注释
    void skipWhitespace();

    // 生成一个指定类型的 Token
    Token makeToken(TokenType type);

    // 解析标识符或关键字
    Token identifier();

    // 解析数字字面量
    Token number();
};
