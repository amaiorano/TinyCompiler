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
	struct Node
	{
		virtual ~Node() = default;
	};
	
	using NodeUniquePtr = std::unique_ptr<Node>;

	struct ProgramNode : Node
	{
		std::string name;
		std::vector<NodeUniquePtr> body;
	};

	struct CallExpressionNode : Node
	{
		std::string name;
		std::vector<NodeUniquePtr> params;
	};

	struct NumberLiteralNode : Node
	{
		int value;
		NumberLiteralNode(int v) : value(v) {}
	};

	namespace
	{
		NodeUniquePtr ParseCallExpression(std::vector<Token>::const_iterator& iter, const std::vector<Token>::const_iterator& endIter)
		{
			auto callExpression = std::make_unique<CallExpressionNode>();

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
						callExpression->params.emplace_back(ParseCallExpression(iter, endIter));
					}
					break;

				case Token::Type::Name:
					throw std::exception("Unexpected name token in argument list");
					break;

				case Token::Type::Number:
					callExpression->params.emplace_back(std::make_unique<NumberLiteralNode>(stoi(iter->value)));
					++iter;
					break;
				}
			}

			throw std::exception("Missing ')' to end call expression");
			return nullptr;
		}
	}

	NodeUniquePtr Parse(const std::vector<Token>& tokens)
	{
		using namespace AST;
		auto programNode = std::make_unique<ProgramNode>();

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

			programNode->body.emplace_back(ParseCallExpression(tokenIter, tokenEnd));
		}

		return move(programNode);
	}

	template <typename TargetNodeType, typename NodeType>
	auto AsNodePtr(NodeType&& node)
	{
		return dynamic_cast<TargetNodeType>(node.get());
	}

	struct Visitor
	{
		virtual void OnVisit(const ProgramNode& program, int depth) {}
		virtual void OnVisit(const CallExpressionNode& callExpression, const Node& parent, int depth) {}
		virtual void OnVisit(const NumberLiteralNode& numberLiteral, const Node& parent, int depth) {}
	};

	void Visit(const NodeUniquePtr& rootNode, const Node* parent, Visitor& visitor, int depth = 0)
	{
		if (auto node = AsNodePtr<const ProgramNode*>(rootNode))
		{
			assert(parent == nullptr);
			visitor.OnVisit(*node, depth);
			for (auto&& n : node->body)
				Visit(n, node, visitor, depth + 1);
		}
		else if (auto node = AsNodePtr<const CallExpressionNode*>(rootNode))
		{
			visitor.OnVisit(*node, *parent, depth);
			for (auto&& n : node->params)
				Visit(n, node, visitor, depth + 1);
		}
		else if (auto node = AsNodePtr<const NumberLiteralNode*>(rootNode))
		{
			visitor.OnVisit(*node, *parent, depth);
		}
		else
		{
			assert(false && "Unhandled node type");
		}
	}
}


void Compile(const std::string& program)
{
	// parsing
	// 1. lexical analysis (tokenizing)
	auto tokens = Tokenize(program);

	// 2. syntactic analysis (create AST)
	auto programNode = AST::Parse(tokens);

	{
		using namespace AST;
		struct PrintAST : Visitor
		{
			void Indent(int depth)
			{
				for (int i = 0; i < depth; ++i)
				{
					std::cout << "  ";
				}
			}

			virtual void OnVisit(const ProgramNode& program, int depth)
			{
				std::cout << "[Program]\n";
			}
			virtual void OnVisit(const CallExpressionNode& callExpression, const Node& parent, int depth)
			{
				Indent(depth);
				std::cout << "[CallExpression] name: " << callExpression.name << '\n';
			}
			virtual void OnVisit(const NumberLiteralNode& numberLiteral, const Node& parent, int depth)
			{
				Indent(depth);
				std::cout << "[NumberLiteral] value: " << numberLiteral.value << '\n';
			}
		};

		auto printAST = PrintAST();
		AST::Visit(programNode, nullptr, printAST);
	}
	
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
	//std::string program = "(add 2 (subtract 4 2))";
	std::string program =
		"(add 2 (subtract 4 2))"
		"(subtract 4 2)"
		;

	Compile(program);

	return 0;
}
