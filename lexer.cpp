#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <cctype>

using namespace std;

namespace parse
{
    /*****************************************************
    ******************   Class Token   *******************
    ******************************************************/

    bool operator==(const Token& lhs, const Token& rhs)
    {
        using namespace token_type;

        if (lhs.index() != rhs.index())
        {
            return false;
        }
        if (lhs.Is<Char>())
        {
            return lhs.As<Char>().value == rhs.As<Char>().value;
        }
        if (lhs.Is<Number>())
        {
            return lhs.As<Number>().value == rhs.As<Number>().value;
        }
        if (lhs.Is<String>())
        {
            return lhs.As<String>().value == rhs.As<String>().value;
        }
        if (lhs.Is<Id>())
        {
            return lhs.As<Id>().value == rhs.As<Id>().value;
        }
        return true;
    }


    bool operator!=(const Token& lhs, const Token& rhs)
    {
        return !(lhs == rhs);
    }


    std::ostream& operator<<(std::ostream& os, const Token& rhs)
    {
        using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

        VALUED_OUTPUT(Number);
        VALUED_OUTPUT(Id);
        VALUED_OUTPUT(String);
        VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

        UNVALUED_OUTPUT(Class);
        UNVALUED_OUTPUT(Return);
        UNVALUED_OUTPUT(If);
        UNVALUED_OUTPUT(Else);
        UNVALUED_OUTPUT(Def);
        UNVALUED_OUTPUT(Newline);
        UNVALUED_OUTPUT(Print);
        UNVALUED_OUTPUT(Indent);
        UNVALUED_OUTPUT(Dedent);
        UNVALUED_OUTPUT(And);
        UNVALUED_OUTPUT(Or);
        UNVALUED_OUTPUT(Not);
        UNVALUED_OUTPUT(Eq);
        UNVALUED_OUTPUT(NotEq);
        UNVALUED_OUTPUT(LessOrEq);
        UNVALUED_OUTPUT(GreaterOrEq);
        UNVALUED_OUTPUT(None);
        UNVALUED_OUTPUT(True);
        UNVALUED_OUTPUT(False);
        UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

        return os << "Unknown token :("sv;
    }



    /*****************************************************
    *******************   Supp Func   ********************
    ******************************************************/

    std::string LoadLiteral(std::istream& input)
    {
        std::string s;

        // Слова должны начинаться с буквы или подчёркивания
        if (NAME_SYMBOLS.find(input.peek()) != std::string::npos)
        {
            // Слова могут содержать символы, подчёркивания и цифры
            while (NAME_SYMBOLS.find(input.peek()) != std::string::npos ||
                   NUMBER_SYMBOLS.count(input.peek()))
            {
                s.push_back(static_cast<char>(input.get()));
            }
        }

        return s;
    }



    /*****************************************************
    ******************   Class Lexer   *******************
    ******************************************************/

    Lexer::Lexer(std::istream& input)
        : token_stream_(input)
        , current_token_()
        , num_indents_(0)
        , token_buffer_()
    {
        current_token_ = NextToken();
    }


    const Token& Lexer::CurrentToken() const
    {
        return current_token_;
    }


    Token Lexer::NextToken()
    {
        std::optional<Token> out_token(std::nullopt);

        // События перед чтением
        //BeforeRead();

        // Проверяем токены в буффере
        out_token = CheckBuffer();

        // Если в буфере нет токенов, то читаем
        if (!out_token.has_value())
        {
            // 2. Пропускаем символы, если необходимо
            SkipSymbols();

            // 3. Читаем токен
            out_token = ReadToken();
        }

        if (out_token.has_value())
        {
            current_token_ = out_token.value();
            return out_token.value();
        }

        throw LexerError("Token not recognized"s); // Если по итогу токен не был считан
    }



    /*****************************************************
    ************    Class Lexer: Read Func   *************
    ******************************************************/

    std::optional<Token> Lexer::ReadToken()
    {
        std::optional<Token> out_token(std::nullopt);

        if (!token_stream_.good())
            return ReadEof();
        if (!out_token)
            out_token = ReadIndentOrDedent();
        if (!out_token)
            out_token = ReadNumber();
        if (!out_token)
            out_token = ReadString();
        if (!out_token)
            out_token = ReadWord();
        if (!out_token)
            out_token = ReadNewline();
        if (!out_token)
            out_token = ReadEof();
        if (!out_token)
            out_token = ReadOperator();

        return out_token;
    }


    std::optional<Token> Lexer::ReadNumber()
    {
        if (!std::isdigit(token_stream_.peek()))
            return std::nullopt;

        std::string parsed_num;
        while (std::isdigit(token_stream_.peek()))
            parsed_num += static_cast<char>(token_stream_.get());

        return token_type::Number{ std::stoi(parsed_num) };
    }


    std::optional<Token> Lexer::ReadString()
    {
        // Строки начинаются с одинарных или двойных кавычек
        if (!(token_stream_.peek() == '\'') && 
            !(token_stream_.peek() == '\"'))
        {
            return std::nullopt;
        }
        // Сохраняем тот тип кавычки с которой начиналась строка
        char closing_char = token_stream_.get();

        auto it = std::istreambuf_iterator<char>(token_stream_);
        auto end = std::istreambuf_iterator<char>();

        std::string s;

        while (true)
        {
            if (it == end || *(it) == '\n')
                throw LexerError("String error: no closing quote");

            const char ch = *it;

            if (ch == closing_char) // С какой кавычки начали, с той и заканчиваем
            {
                ++it;
                break;
            }
            else if (ch == '\\') // если найден символ экранирования
            {
                ++it;

                if (it == end)
                    throw LexerError("String error: no closing quote");

                const char escaped_char = *(it);
                switch (escaped_char)
                {
                case 'n':
                    s.push_back('\n');
                    break;
                case 't':
                    s.push_back('\t');
                    break;
                case 'r':
                    s.push_back('\r');
                    break;
                case '"':
                    s.push_back('"');
                    break;
                case '\'':
                    s.push_back('\'');
                    break;
                case '\\':
                    s.push_back('\\');
                    break;
                default:
                    throw LexerError("String error: Unrecognized escape sequence \\"s + escaped_char);
                }
            }
            else if (ch == '\n' || ch == '\r')
            {
                throw LexerError("String error: unexpected end of line"s);
            }
            else
            {
                s.push_back(ch);
            }

            ++it;
        }

        return token_type::String{ s };
    }


    std::optional<Token> Lexer::ReadWord()
    {
        std::string str(LoadLiteral(token_stream_));

        if (str.empty())
            return std::nullopt;
        else if (KEYWORDS.count(str))
            return TOKEN_STRING.at(str);
        else
            return token_type::Id{ str };
    }


    std::optional<Token> Lexer::ReadNewline()
    {
        if (token_stream_.peek() == '\n' && 
            !current_token_.Is<token_type::Newline>())
        {
            token_stream_.get();
            return token_type::Newline();
        }
        else
        {
            return std::nullopt;
        }
    }


    std::optional<Token> Lexer::ReadEof()
    {
        if (token_stream_.eof())
        {
            // Проверяем, не остались ли отступы
            if (num_indents_ != 0)
            {
                // заполняем буффер оставшимися токенами
                for (; num_indents_ > 1; num_indents_--)
                    token_buffer_.push(token_type::Dedent());

                token_buffer_.push(token_type::Eof());
                num_indents_--;
                return token_type::Dedent();
            }
            else if (!IsNewline() && !current_token_.Is<token_type::Dedent>() && !current_token_.Is<token_type::Eof>())
            {
                token_buffer_.push(token_type::Eof());
                return token_type::Newline();
            }
            else
            {
                return token_type::Eof();
            }
        }
        else
            return std::nullopt;
    }


    std::optional<Token> Lexer::ReadOperator()
    {
        if (!SYMBOLS.count(token_stream_.peek()))
            return std::nullopt;

        std::optional<Token> out_token(std::nullopt);

        char c1 = token_stream_.get();

        // Если следом идёт ещё один символ, то проверяем на двухсимвольные операторы
        if (SYMBOLS.count(token_stream_.peek()))
        {
            char c2 = token_stream_.get();
            std::string str;
            str.push_back(c1);
            str.push_back(c2);

            if (TOKEN_STRING.count(str))
                out_token = TOKEN_STRING.at(str);
            else
                token_stream_.putback(c2);
        }

        if (!out_token)
            out_token = token_type::Char{ c1 };

        return out_token;
    }


    std::optional<Token> Lexer::ReadIndentOrDedent()
    {
        if (!IsNewline())
            return std::nullopt;

        std::optional<Token> out_token(std::nullopt);

        // Нужно подсчитать, изменилось ли количество отступов
        unsigned int num_indents = CountIndents();

        if (num_indents > num_indents_) // Если отступов стало больше
        {
            // Нужно возвратить из функции token_type::Indent и 
            // если количество отступов увеличилось больше чем на 1, записываем остальные в буфер

            // 1. Определяем на сколько отступов стало больше
            num_indents -= num_indents_;

            // 2. Изменяем счётчик отступов
            num_indents_ += num_indents;

            // 3. Добавляем лишние токены в буфер
            for (; num_indents > 1; num_indents--)
                token_buffer_.push(token_type::Indent());

            // 4. Присваиваем значение переменной для вывода
            out_token = token_type::Indent();
        }
        else if (num_indents < num_indents_) // Если отступов стало меньше
        {
            // Нужно возвратить из функции token_type::Dedent и 
            // если количество отступов уменьшилось больше чем на 1, записываем остальные в буфер

            // 1. Определяем на сколько отступов стало меньше
            num_indents = num_indents_ - num_indents;

            // 2. Изменяем счётчик отступов
            num_indents_ -= num_indents;

            // 3. Добавляем лишние токены в буфер
            for (; num_indents > 1; num_indents--)
                token_buffer_.push(token_type::Dedent());

            // 4. Присваиваем значение переменной для вывода
            out_token = token_type::Dedent();
        }

        return out_token;
    }



    /*****************************************************
     *********    Class Lexer: Helper methods   **********
     ******************************************************/

    void Lexer::SkipSymbols()
    {
        /* 
        *   Пропускает следущие символы:
        *   1. Пробелы, не относящиеся к отступам;
        *   2. Комментарии
        *   3. Пустые строки
        *   4. Лишние отступы
        */
        if (!token_stream_.good())
            return;

        if (token_stream_.peek() == ' ') // Пропуск пробелов
        {
            if (IsNewline()) // если начало строки или после отступа
            {
                char c = token_stream_.get();
                if (token_stream_.peek() == ' ') // Если это отступ
                    token_stream_.putback(c);
            }
            else
            {
                while (token_stream_.peek() == ' ')
                    token_stream_.get();
            }
        }
        if (token_stream_.peek() == '#') // Пропуск комментария
        {
            token_stream_.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            if (!IsNewline())
                token_stream_.putback('\n');
        }
        if (token_stream_.peek() == ' ' || token_stream_.peek() == '\n') // Пропуск пустых строк
        {
            if (IsNewline())
            {
                bool is_empty = true;

                while (is_empty)
                {
                    // 1. Запоминаем позицию потока
                    size_t pos = token_stream_.tellg();

                    // 2. Читаем строку из потока
                    std::string str;
                    getline(token_stream_, str);

                    if (IsEmptyString(str)) // Проверяем, состоит ли строка только из пробелов
                    {
                        is_empty = true;
                    }
                    else if (str.find('#') != std::string::npos) // Проверяем, состоит ли строка только из пробелов и комментариев
                    {
                        // Отсекаем комментарий
                        str.resize(str.find('#'));

                        // Проверяем, есть ли какие то либо символы до комментария
                        is_empty = IsEmptyString(str);
                    }
                    else if (str == "\n"s) // Состоит только из символа переноса строки
                    {
                        is_empty = true;
                    }
                    else
                    {
                        is_empty = false;
                    }

                    if(token_stream_.eof()) // Если по итогу дочитали до конца файла
                        break;

                    if (!is_empty) // Если строка не пуста, то возвращаем указатель назад
                    {
                        token_stream_.seekg(pos);
                    }
                }

            }
        }
        //if (IsNewline()) // Пропуск лишних отступов
        //{
        //    size_t pos = token_stream_.tellg();
        //    int num_indents = CountIndents();

        //    if (!(num_indents == num_indents_))
        //        token_stream_.seekg(pos);
        //}
    }


    std::optional<Token> Lexer::CheckBuffer()
    {
        std::optional<Token> out_token(std::nullopt);

        if (!token_buffer_.empty())
        {
            out_token = token_buffer_.front(); // записываем токен из буфера
            token_buffer_.pop(); // удаляем считанный токен
        }

        return out_token;
    }


    bool Lexer::IsNewline() const
    {
        return current_token_.Is<token_type::Newline>();
    }


    bool Lexer::IsEmptyString(std::string& str)
    {
        return str.empty() ||
            str.find_first_not_of(" \n") == std::string::npos;
    }


    int Lexer::CountIndents() const
    {
        if (IsNewline()) // Проверяем, начало ли строки
        {
            unsigned int num_indents = 0;

            while (token_stream_.peek() == ' ')
            {
                token_stream_.get(); // Считываем первый пробел
                if (token_stream_.peek() == ' ')
                {
                    token_stream_.get(); // Считываем второй пробел
                    num_indents++;
                }
                else
                {
                    break;
                }
            }

            return num_indents;
        }
        else
        {
            return -1;
        }
    }

}  // namespace parse