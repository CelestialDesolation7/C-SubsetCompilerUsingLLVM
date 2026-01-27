#include "include/lexer.h"
#include <unordered_map>

// 关键字映射表：将标识符字符串映射到对应的 TokenType
static const std::unordered_map<std::string, TokenType> keywords = {
    {"int", TokenType::INT},
    {"void", TokenType::VOID},
    {"if", TokenType::IF},
    {"else", TokenType::ELSE},
    {"while", TokenType::WHILE},
    {"return", TokenType::RETURN},
    {"break", TokenType::BREAK},
    {"continue", TokenType::CONTINUE}};

// 构造函数：初始化源代码和行号
Lexer::Lexer(const std::string &source) : src(source), pos(0), line(1) {}

// peek：返回当前字符但不推进位置；若到达末尾，返回 '\0'
char Lexer::peek() const
{
    return (pos >= src.size()) ? '\0' : src[pos];
}

// advance：消费当前字符并返回它，同时将扫描位置 pos 前移一位
char Lexer::advance()
{
    char c = peek();
    ++pos;
    return c;
}

// skipWhitespace：跳过空白字符（空格、制表符、换行）和更新行号
void Lexer::skipWhitespace()
{
    while (isspace(peek()))
    {
        if (peek() == '\n')
            ++line; // 行号递增
        advance();  // 消费空白
    }
}

// makeToken：基于最近消费的单字符生成 Token，lexeme 为该字符
Token Lexer::makeToken(TokenType type)
{
    std::string text(1, src[pos - 1]); // string(1, char) 将一个字符转换为字符串
    return {type, text, line};
}

// identifier：解析标识符或关键字，支持字母、数字和下划线
Token Lexer::identifier()
{
    size_t start = pos - 1; // 这个值是当前字符的位置
    // 为什么需要-1?因为主函数中调用nextToken()时，已经advance()了一次
    while (isalnum(peek()) || peek() == '_')
        advance();
    std::string lex = src.substr(start, pos - start);
    auto it = keywords.find(lex);
    TokenType type = (it != keywords.end()) ? it->second : TokenType::ID;
    // 如果找到关键字，则返回关键字类型，否则返回标识符类型(ID)
    return {type, lex, line};
}

// number：解析数字字面量，连续的数字字符
Token Lexer::number()
{
    size_t start = pos - 1;
    while (isdigit(peek()))
        advance();
    std::string lex = src.substr(start, pos - start);
    return {TokenType::NUMBER, lex, line};
}

// nextToken：主入口，跳过空白和注释，识别并返回下一个 Token
Token Lexer::nextToken()
{
    // 跳过所有空白和单行注释
    for (;;) // 无限循环
    {
        skipWhitespace(); // 跳过空白字符
        if (peek() == '/' && pos + 1 < src.size() && src[pos + 1] == '/')
        {
            // 跳过单行注释
            advance(); // 消费 '/'
            advance(); // 消费第二个 '/'
            // 跳至行尾或文件末尾
            while (peek() != '\n' && peek() != '\0')
                advance();
            continue;
        }
        break;
    }

    char c = advance();
    switch (c)
    {
    case '\0':
        return {TokenType::END, "", line};
    case '+':
        return makeToken(TokenType::PLUS);
    case '-':
        return makeToken(TokenType::MINUS);
    case '*':
        return makeToken(TokenType::TIMES);
    case '/':
        return makeToken(TokenType::DIV);
    case '%':
        return makeToken(TokenType::MOD);

    case '=':
        if (peek() == '=')
        {
            advance();
            return {TokenType::EQ, "==", line};
        }
        return makeToken(TokenType::ASSIGN);

    case '<':
        if (peek() == '=')
        {
            advance();
            return {TokenType::LE, "<=", line};
        }
        return makeToken(TokenType::LT);

    case '>':
        if (peek() == '=')
        {
            advance();
            return {TokenType::GE, ">=", line};
        }
        return makeToken(TokenType::GT);

    case '!':
        if (peek() == '=')
        {
            advance();
            return {TokenType::NE, "!=", line};
        }
        return makeToken(TokenType::NOT);

    case '&':
        if (peek() == '&')
        {
            advance();
            return {TokenType::AND, "&&", line};
        }
        break;

    case '|':
        if (peek() == '|')
        {
            advance();
            return {TokenType::OR, "||", line};
        }
        break;

    case '(':
        return makeToken(TokenType::LPAREN);
    case ')':
        return makeToken(TokenType::RPAREN);
    case '{':
        return makeToken(TokenType::LBRACE);
    case '}':
        return makeToken(TokenType::RBRACE);
    case ';':
        return makeToken(TokenType::SEMI);
    case ',':
        return makeToken(TokenType::COMMA);

    default:
        if (isalpha(c) || c == '_')
            return identifier();
        if (isdigit(c))
            return number();
        break;
    }

    // 其他未识别字符
    return {TokenType::UNKNOWN, std::string(1, c), line};
}
