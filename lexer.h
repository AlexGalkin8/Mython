#pragma once

#include <iosfwd>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <set>
#include <map>
#include <queue>

/*
* Файл содержит структуры лексем и  класс Lexer, выполняющий лексический разбор программы.
* Класс Lexer - принимает ссылку на поток ввода, из которого считывает текст программы,
* и выдаёт последовательность лексем программы на языке Mython.
*/


namespace parse
{
    const std::set<char> NUMBER_SYMBOLS =
    { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' };

    // Список поддерживаемых символов
    const std::set<char> SYMBOLS =
    { '-', '+', '=', '>', '<', '!', '*', '/', ';', ',', '.', '(', ')', ':', '$',
      '%', '|', '\\', '[', ']', '{', '}', '/', '?', '&', '^', '@'};

    // Список ключевых слов
    const std::set<std::string> KEYWORDS =
    { "class", "return", "def", "None", "if", "else", "True", "False", "and", "or", "not", "print" };

    const std::set<std::string> LOGIC_OPERATOR =
    { "==", "!=", "<=" , ">=" };

    // Идентификаторы и ключевые слова состоят из заглавных и строчных букв,а также с подчёркивания
    const std::string NAME_SYMBOLS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopwrstuvwxyz_";


    class LexerError : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };



    namespace token_type   // Содержит лексемы 
    {
        struct Number // Лексема «число»
        {
            int value;   // число
        };

        struct Id // Лексема «идентификатор»
        {
            std::string value;  // Имя идентификатора
        };

        struct Char // Лексема «символ»
        {
            char value;  // код символа
        };

        struct String // Лексема «строковая константа»
        {
            std::string value;
        };

        struct Class {};        // Лексема «class»
        struct Return {};       // Лексема «return»
        struct If {};           // Лексема «if»
        struct Else {};         // Лексема «else»
        struct Def {};          // Лексема «def»
        struct Newline {};      // Лексема «конец строки»
        struct Print {};        // Лексема «print»
        struct Indent {};       // Лексема «увеличение отступа», соответствует двум пробелам
        struct Dedent {};       // Лексема «уменьшение отступа»
        struct Eof {};          // Лексема «конец файла»
        struct And {};          // Лексема «and»
        struct Or {};           // Лексема «or»
        struct Not {};          // Лексема «not»
        struct Eq {};           // Лексема «==»
        struct NotEq {};        // Лексема «!=»
        struct LessOrEq {};     // Лексема «<=»
        struct GreaterOrEq {};  // Лексема «>=»
        struct None {};         // Лексема «None»
        struct True {};         // Лексема «True»
        struct False {};        // Лексема «False»

    }  // namespace token_type



    using TokenBase
        = std::variant<token_type::Newline, token_type::Id, token_type::Char, token_type::String,
        token_type::Class, token_type::Return, token_type::If, token_type::Else,
        token_type::Def, token_type::Eof, token_type::Print, token_type::Indent,
        token_type::Dedent, token_type::And, token_type::Or, token_type::Not,
        token_type::Eq, token_type::NotEq, token_type::LessOrEq, token_type::GreaterOrEq,
        token_type::None, token_type::True, token_type::False, token_type::Number>;



    struct Token : public TokenBase
    {
        using TokenBase::TokenBase;

        template <typename T>
        [[nodiscard]] bool Is() const
        {
            return std::holds_alternative<T>(*this);
        }

        template <typename T>
        [[nodiscard]] const T& As() const
        {
            return std::get<T>(*this);
        }

        template <typename T>
        [[nodiscard]] const T* TryAs() const
        {
            return std::get_if<T>(this);
        }
    };

    bool operator==(const Token& lhs, const Token& rhs);
    bool operator!=(const Token& lhs, const Token& rhs);

    std::ostream& operator<<(std::ostream& os, const Token& rhs);

    // Для определения типа токена
    const std::map<std::string, Token> TOKEN_STRING
        {
            { "class", token_type::Class() },
            { "return", token_type::Return() },
            { "if", token_type::If() },
            { "else", token_type::Else() },
            { "def", token_type::Def() },
            { "print", token_type::Print() },
            { "and", token_type::And() },
            { "or", token_type::Or() },
            { "not", token_type::Not() },
            { "None", token_type::None() },
            { "True", token_type::True() },
            { "False", token_type::False() },
            { "==", token_type::Eq() },
            { "!=", token_type::NotEq() },
            { "<=", token_type::LessOrEq() },
            { ">=", token_type::GreaterOrEq() }
        };



    /*********************   Вспомогательные функции   *********************/

    // Вспомогательная функция для чтения идентификаторов и ключевых слов
    std::string LoadLiteral(std::istream& input);



    class Lexer
    {
    public:
        explicit Lexer(std::istream& input);

        // Возвращает ссылку на текущий токен
        [[nodiscard]] const Token& CurrentToken() const;

        // Возвращает следующий токен
        Token NextToken();

        // Если текущий токен имеет тип T, метод возвращает ссылку на него.
        // В противном случае метод выбрасывает исключение LexerError
        template <typename T>
        const T& Expect() const;

        // Метод проверяет, что текущий токен имеет тип T, а сам токен содержит значение value.
        // В противном случае метод выбрасывает исключение LexerError
        template <typename T, typename U>
        void Expect(const U& value) const;

        // Если следующий токен имеет тип T, метод во возвращает ссылку на него.
        // В противном случае метод выбрасывает исключение LexerError
        template <typename T>
        const T& ExpectNext();

        // Метод проверяет, что следующий токен имеет тип T, а сам токен содержит значение value.
        // В противном случае метод выбрасывает исключение LexerError
        template <typename T, typename U>
        void ExpectNext(const U& value);

    private:
        // Основной метод чтения токена из потока
        std::optional<Token> ReadToken();

        std::optional<Token> ReadNumber();           // Метод для чтения чисел
        std::optional<Token> ReadString();           // Метод для чтения строк
        std::optional<Token> ReadWord();             // Метод для чтения идентификатора или ключевого слова
        std::optional<Token> ReadNewline();          // Метод для чтения лексемы переноса строки
        std::optional<Token> ReadEof();              // Метод для чтения лексемы конца файла
        std::optional<Token> ReadOperator();         // Метод для чтения символов и логических операторов
        std::optional<Token> ReadIndentOrDedent();   // Метод для чтения лексем уменьшения отступа отступа
    private:
        void SkipSymbols();                          // Пропускает ненужные символы перед чтением токена
        std::optional<Token> CheckBuffer();          // При наличии возвращяет токены из буфера
        bool IsNewline() const;                      // Проверяет поток на чтение отступа
        static bool IsEmptyString(std::string& str); // Проверяет, пустая ли строка
        int CountIndents() const;                    // Считает количество отступов от начала строки

    private:
        std::istream& token_stream_;   // Поток, из которого считаются токены
        Token         current_token_;  // Буфер для последнего считанного токена
        unsigned int  num_indents_;    // Количество отступов, с начала строки

        // Буфер токенов. Служит для вывода ранее считанных токенов.
        std::queue<Token> token_buffer_;
    };



    template <typename T>
    const T& Lexer::Expect() const
    {
        if (CurrentToken().Is<T>())
            return CurrentToken().As<T>();
        else
            throw LexerError("The current token is not a passed type");
    }


    template <typename T, typename U>
    void Lexer::Expect(const U& value) const
    {
        if (!(Expect<T>().value == value))
            throw LexerError("The current token does not contain the intended value");
    }


    template <typename T>
    const T& Lexer::ExpectNext()
    {
        NextToken();
        return Expect<T>();
    }


    template <typename T, typename U>
    void Lexer::ExpectNext(const U& value)
    {
        NextToken();
        return Expect<T>(value);
    }

}  // namespace parse