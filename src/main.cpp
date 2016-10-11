#include <string>
#include <vector>

#include "variant_match.h"

struct Token
{
	enum class Type { Paren, Name, Number };
	Type type;
	std::string value;
};

std::string CharToString(char c) { return std::string{ 1, c }; }

std::vector<Token> Tokenize(const std::string text)
{
	std::vector<Token> tokens;

	struct Looking {};
	struct InString { std::string value; };
	struct InNumber { std::string value; };

	auto state = std::experimental::variant<Looking, InString, InNumber>{ Looking{} };

	size_t i = 0;
	while (i < text.size())
	{
		const char c = text[i];
		const auto cs = std::string(1, c);

		match(state,
			[&](Looking& looking)
			{
				if (c == ' ' || c == '\t' || c == '\n') // Whitespace
				{
					++i;
				}
				else if (c == '(' || c == ')')
				{
					tokens.emplace_back(Token{ Token::Type::Paren, cs });
					++i;
				}
				else if (isalpha(c))
				{
					state = InString{};
				}
				else if (isdigit(c))
				{
					state = InNumber{};
				}
				else
				{
					throw std::exception("Unexpected character");
				}
			},
			[&](InString& inString)
			{
				if (isalpha(c))
				{
					inString.value += cs;
					++i;
				}
				else
				{
					tokens.emplace_back(Token{ Token::Type::Name, inString.value });
					state = Looking{};
				}
			},
			[&](InNumber& inNumber)
			{
				if (isdigit(c))
				{
					inNumber.value += cs;
					++i;
				}
				else
				{
					tokens.emplace_back(Token{ Token::Type::Number, inNumber.value });
					state = Looking{};
				}
			}
		);
	}

	return tokens;
}

void Compile(const std::string& program)
{
	// parsing
	// 1. lexical analysis (tokenizing)
	auto tokens = Tokenize(program);

	// 2. syntactic analysis (create AST)


	// transformation


}


int main(int argc, char* argv[])
{
/*
 *                  LISP                      C
 *
 *   2 + 2          (add 2 2)                 add(2, 2)
 *   4 - 2          (subtract 4 2)            subtract(4, 2)
 *   2 + (4 - 2)    (add 2 (subtract 4 2))    add(2, subtract(4, 2))
 */
	std::string program = "(add 2 2)";

	Compile(program);

	return 0;
}
