#include <string>
#include <vector>
#include <memory>
#include <iostream>

#include "variant_match.h"

struct Token
{
	enum class Type { Paren, Name, Number };
	Type type;
	std::string value;
};

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



namespace AST
{
	struct Visitor
	{
		virtual void OnVisit(struct Program& program) = 0;
		virtual void OnVisit(struct CallExpression& callExpression) = 0;
		virtual void OnVisit(struct NumberLiteral& numberLiteral) = 0;
	};

	struct Node
	{
		using NodeUniquePtr = std::unique_ptr<Node>;
	
		Node* parent = nullptr;
		std::vector<NodeUniquePtr> children;

		void AddChild(NodeUniquePtr node)
		{
			node->parent = this;
			children.emplace_back(move(node));
		}

		virtual void Visit(Visitor& visitor) = 0;
	};
	
	using NodeUniquePtr = std::unique_ptr<Node>;

	struct Program : Node
	{
		std::string name;
		//std::vector<NodeUniquePtr> body;

		void Visit(Visitor& visitor) override
		{
			visitor.OnVisit(*this);
			for (auto& node : children)
				node->Visit(visitor);
		}
	};

	struct CallExpression : Node
	{
		std::string name;
		//std::vector<NodeUniquePtr> params;

		void Visit(Visitor& visitor) override
		{
			visitor.OnVisit(*this);
			for (auto& param : children)
				param->Visit(visitor);
		}
	};

	struct NumberLiteral : Node
	{
		int value;
		NumberLiteral(int v) : value(v) {}

		void Visit(Visitor& visitor) override
		{
			visitor.OnVisit(*this);
		}
	};

	//template <typename CurrIter, typename EndIter>
	NodeUniquePtr ParseCallExpression(std::vector<Token>::const_iterator& iter, const std::vector<Token>::const_iterator& endIter)
	{
		auto callExpression = std::make_unique<CallExpression>();

		const auto& firstToken = *iter;
		if (firstToken.type != Token::Type::Name)
			throw std::exception("Expecting function name immediately after '('");

		callExpression->name = iter->value;
		++iter;

		while (iter != endIter)
		{
			switch (iter->type)
			{
			case Token::Type::Paren:
				if (iter->value == ")")
				{
					++iter;
					return move(callExpression);
				}
				else
				{
					++iter;
					callExpression->AddChild(ParseCallExpression(iter, endIter));
				}
				break;

			case Token::Type::Name:
				throw std::exception("Unexpected name token in argument list");
				break;

			case Token::Type::Number:
				callExpression->AddChild(std::make_unique<NumberLiteral>(stoi(iter->value)));
				++iter;
				break;
			}
		}

		throw std::exception("Missing ')' to end call expression");
		return nullptr;
	}

	NodeUniquePtr Parse(const std::vector<Token>& tokens)
	{
		using namespace AST;
		auto programNode = std::make_unique<Program>();

		auto tokenIter = begin(tokens);
		auto tokenEnd = end(tokens);

		// Loop here for each top-level call expression
		// e.g.
		//		(add 1 2)
		//		(sub 3 4)
		while (tokenIter != tokenEnd)
		{
			const auto& firstToken = *tokenIter;
			if (!(firstToken.type == Token::Type::Paren && firstToken.value == "("))
				throw std::exception("Program must start with '('");
			++tokenIter;

			programNode->AddChild(ParseCallExpression(tokenIter, tokenEnd));
		}

		return move(programNode);
	}
}


void Compile(const std::string& program)
{
	// parsing
	// 1. lexical analysis (tokenizing)
	auto tokens = Tokenize(program);

	// 2. syntactic analysis (create AST)
	auto programNode = AST::Parse(tokens);

	struct PrintAST : AST::Visitor
	{
		virtual void OnVisit(struct AST::Program& program)
		{
			std::cout << "[Program]\n";
		}
		virtual void OnVisit(struct AST::CallExpression& callExpression)
		{
			std::cout << "\t[CallExpression] name: " << callExpression.name << "\n\t";
		}
		virtual void OnVisit(struct AST::NumberLiteral& numberLiteral)
		{
			std::cout << "[NumberLiteral] value: " << numberLiteral.value << "\n";
		}
	};

	auto printAST = PrintAST();
	programNode->Visit(printAST);


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
	//std::string program = "(add 2 2)";
		//"(add 2 (subtract 4 2))"
	std::string program =
		"(add 2 (subtract 4 2))"
		"(subtract 4 2)"
		;


	Compile(program);

	return 0;
}
