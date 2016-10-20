#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <sstream>
#include <map>
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

namespace CommonAst
{
	struct Node
	{
		virtual ~Node() = default;
	};

	using NodeUniquePtr = std::unique_ptr<Node>;

	template <typename TargetNodeType, typename NodeType>
	auto AsNodePtr(NodeType&& node)
	{
		return dynamic_cast<TargetNodeType>(node.get());
	}
}

namespace LispAst
{
	using namespace CommonAst;

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
						return std::move(callExpression);
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
		using namespace LispAst;
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

		return std::move(programNode);
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

	void PrintAst(const NodeUniquePtr& lispAst, std::ostream& os)
	{
		struct PrintAST : Visitor
		{
			std::ostream& os;
			PrintAST(std::ostream& os) : os(os) {}

			void Indent(int depth)
			{
				for (int i = 0; i < depth; ++i)
				{
					os << "  ";
				}
			}

			virtual void OnVisit(const ProgramNode& program, int depth)
			{
				os << "[Program]\n";
			}
			virtual void OnVisit(const CallExpressionNode& callExpression, const Node& parent, int depth)
			{
				Indent(depth);
				os << "[CallExpression] name: " << callExpression.name << '\n';
			}
			virtual void OnVisit(const NumberLiteralNode& numberLiteral, const Node& parent, int depth)
			{
				Indent(depth);
				os << "[NumberLiteral] value: " << numberLiteral.value << '\n';
			}
		};

		auto printAST = PrintAST(os);
		LispAst::Visit(lispAst, nullptr, printAST);
	}
} // namespace LispAst

namespace CppAst
{
	using namespace CommonAst;

	struct ProgramNode : Node
	{
		std::string name;
		std::vector<NodeUniquePtr> body;
	};

	struct IdentifierNode : Node
	{
		std::string name;
		IdentifierNode(std::string name) : name(std::move(name)) {}
	};

	struct NumberLiteralNode : Node
	{
		int value;
		NumberLiteralNode(int v) : value(v) {}
	};

	struct CallExpressionNode : Node
	{
		//NodeUniquePtr callee;
		std::unique_ptr<IdentifierNode> callee;
		std::vector<NodeUniquePtr> params;
	};

	struct ExpressionStatementNode : Node
	{
		//NodeUniquePtr expression;
		std::unique_ptr<CallExpressionNode> expression;
	};

	template <typename NodeUniquePtrType> // Note: need template only because 'callee' and 'expression' are not NodeUniquePtrs
	void PrintAst(const NodeUniquePtrType& rootNode, std::ostream& os, int depth=0)
	{
		auto Indent = [&os](int depth)
		{
			for (int i = 0; i < depth; ++i)
			{
				os << "  ";
			}
		};

		if (auto node = AsNodePtr<const ProgramNode*>(rootNode))
		{
			Indent(depth); os << "[Program]\n";
			Indent(depth); os << " Body:\n";
			for (auto&& bodyNode : node->body)
			{
				PrintAst(bodyNode, os, depth + 1);
			}
		}
		else if (auto node = AsNodePtr<const ExpressionStatementNode*>(rootNode))
		{
			Indent(depth); os << "[ExpressionStatement]\n";
			Indent(depth); os << " Expression:\n";
			PrintAst(node->expression, os, depth + 1);
		}
		else if (auto node = AsNodePtr<const CallExpressionNode*>(rootNode))
		{
			Indent(depth); os << "[CallExpression]\n";
			Indent(depth); os << " Callee:\n";
			PrintAst(node->callee, os, depth + 1);
			Indent(depth); os << " Params:\n";
			for (auto&& param : node->params)
			{
				PrintAst(param, os, depth + 1);
			}

		}
		else if (auto node = AsNodePtr<const IdentifierNode*>(rootNode))
		{
			Indent(depth); os << "[Identifier] name: " << node->name << '\n';
		}
		else if (auto node = AsNodePtr<const NumberLiteralNode*>(rootNode))
		{
			Indent(depth); os << "[NumberLiteralNode] value: " << node->value << '\n';
		}
		else
		{
			assert(false && "Unhandled node type");
		}
	}
} // namespace CppAst

// std::less<reference_wrapper<T>> doesn't work in containers like map, so use this instead
template <typename T>
struct reference_wrapper_less
{
	bool operator()(const std::reference_wrapper<T>& lhs, const std::reference_wrapper<T>& rhs) const { return &lhs.get() < &rhs.get(); }
};

CppAst::NodeUniquePtr TransformLispAstToCppAst(const LispAst::NodeUniquePtr& lispAst)
{
	struct Transformer : LispAst::Visitor
	{
		CppAst::NodeUniquePtr m_programNode;

		// Map of Lisp parent nodes to Cpp vectors of nodes. This basically allows us to find the relevant parent
		// vector to add a Cpp node to from within visitor callbacks.
		std::map<
			std::reference_wrapper<const LispAst::Node>,
			std::reference_wrapper<std::vector<CppAst::NodeUniquePtr>>,
			reference_wrapper_less<const LispAst::Node>
		> m_context;

		std::vector<CppAst::NodeUniquePtr>& GetContextVector(const LispAst::Node& lispNode)
		{
			auto iter = m_context.find(lispNode);
			assert(iter != m_context.end());
			return iter->second.get();
		}

		virtual void OnVisit(const LispAst::ProgramNode& lispProgramNode, int depth)
		{
			assert(m_programNode == nullptr);
			auto cppProgramNode = std::make_unique<CppAst::ProgramNode>();
			m_context.emplace(std::cref(lispProgramNode), std::ref(cppProgramNode->body));
			m_programNode = std::move(cppProgramNode);
		}

		virtual void OnVisit(const LispAst::CallExpressionNode& lispCallExpressionNode, const LispAst::Node& parent, int depth)
		{
			assert(m_programNode);

			// Create call expression with nested identifier and no parameters
			auto callExpressionNode = std::make_unique<CppAst::CallExpressionNode>();
			callExpressionNode->callee = std::make_unique<CppAst::IdentifierNode>(lispCallExpressionNode.name);

			// Add mapping from the Lisp CallExpressionNode to the parameter vector of our new Cpp CallExpressionNode.
			m_context.emplace(std::cref(lispCallExpressionNode), std::ref(callExpressionNode->params));

			auto newNode = [&]() -> CppAst::NodeUniquePtr
			{
				// If parent is not a CallExpression, we wrap up our Cpp CallExpression node with an ExpressionStatement,
				// because in C++, top-level call expressions are statements.
				if (!dynamic_cast<const LispAst::CallExpressionNode*>(&parent))
				{
					auto expressionStatementNode = std::make_unique<CppAst::ExpressionStatementNode>();
					expressionStatementNode->expression = std::move(callExpressionNode);
					//callExpressionNode = std::move(expressionStatementNode);
					return std::move(expressionStatementNode);
				}
				else
				{
					return std::move(callExpressionNode);
				}
			} ();

			// Add new node to parent's context
			GetContextVector(parent).push_back(std::move(newNode));
		}

		virtual void OnVisit(const LispAst::NumberLiteralNode& lispNumberLiteralNode, const LispAst::Node& parent, int depth)
		{
			assert(m_programNode);
			auto newNode = std::make_unique<CppAst::NumberLiteralNode>(lispNumberLiteralNode.value);
			GetContextVector(parent).push_back(std::move(newNode));
		}
	};

	auto transformer = Transformer();
	LispAst::Visit(lispAst, nullptr, transformer);

	return std::move(transformer.m_programNode);
}

namespace impl
{
	template <typename NodeUniquePtrType>
	void GenerateCppCodeImpl(const NodeUniquePtrType& rootNode, std::ostream& os, int depth = 0)
	{
		using namespace CppAst;
		
		auto Indent = [&](int depth)
		{
			for (int i = 0; i < depth; ++i)
			{
				os << "  ";
			}
		};

		if (auto node = AsNodePtr<const ProgramNode*>(rootNode))
		{
			os << "int main()\n";
			os << "{\n";
			for (auto&& bodyNode : node->body)
			{
				GenerateCppCodeImpl(bodyNode, os, depth + 1);
			}
			os << "}\n";
		}
		else if (auto node = AsNodePtr<const ExpressionStatementNode*>(rootNode))
		{
			Indent(depth);
			GenerateCppCodeImpl(node->expression, os, depth + 1);
			os << ";\n";
		}
		else if (auto node = AsNodePtr<const CallExpressionNode*>(rootNode))
		{
			GenerateCppCodeImpl(node->callee, os, depth + 1);
			os << "(";
			for (auto iter = begin(node->params); iter != end(node->params); ++iter)
			{
				auto&& param = *iter;
				GenerateCppCodeImpl(param, os, depth + 1);
				if ((iter + 1) != end(node->params))
				{
					GenerateCppCodeImpl(param, os, depth + 1);
					os << ", ";
				}
			}
			os << ")";
		}
		else if (auto node = AsNodePtr<const IdentifierNode*>(rootNode))
		{
			os << node->name;
		}
		else if (auto node = AsNodePtr<const NumberLiteralNode*>(rootNode))
		{
			os << node->value;
		}
		else
		{
			assert(false && "Unhandled node type");
		}
	}
} // namespace impl

std::string GenerateCppCode(const CppAst::NodeUniquePtr& cppAst)
{
	std::stringstream sstream;
	impl::GenerateCppCodeImpl(cppAst, sstream);
	return sstream.str();
}

void Compile(const std::string& lispCode)
{
	const bool printAsts = true;

	std::cout << "Input Lisp code:\n" << lispCode << "\n";

	/////////////////////
	// Parsing
	/////////////////////

	// 1. lexical analysis (tokenizing)
	auto tokens = Tokenize(lispCode);

	// 2. syntactic analysis (create the Lisp AST)
	auto lispAst = LispAst::Parse(tokens);

	if (printAsts)
	{
		std::cout << "Lisp AST:\n";
		LispAst::PrintAst(lispAst, std::cout);
		std::cout << '\n';
	}

	/////////////////////
	// Transformation
	/////////////////////

	auto cppAst = TransformLispAstToCppAst(lispAst);

	if (printAsts)
	{
		std::cout << "Cpp AST:\n";
		CppAst::PrintAst(cppAst, std::cout);
		std::cout << '\n';
	}

	/////////////////////
	// Code Generation
	/////////////////////

	auto cppCode = GenerateCppCode(cppAst);
	std::cout << "Generated Cpp Code:\n" << cppCode << '\n';
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
	std::string lispCode =
		"(add 2 (subtract 4 2))\n"
		"(subtract 3 7)\n"
		"(foo (bar (len 2 3)))\n"
		;

	Compile(lispCode);

	return 0;
}
