/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <libsolidity/analysis/SyntaxChecker.h>
#include <memory>
#include <libsolidity/ast/AST.h>
#include <libsolidity/analysis/SemVerHandler.h>
#include <libsolidity/interface/ErrorReporter.h>
#include <libsolidity/interface/Version.h>

using namespace std;
using namespace dev;
using namespace dev::solidity;


bool SyntaxChecker::checkSyntax(ASTNode const& _astRoot)
{
	_astRoot.accept(*this);
	return Error::containsOnlyWarnings(m_errorReporter.errors());
}

bool SyntaxChecker::visit(SourceUnit const&)
{
	m_versionPragmaFound = false;
	return true;
}

void SyntaxChecker::endVisit(SourceUnit const& _sourceUnit)
{
	if (!m_versionPragmaFound)
	{
		string errorString("Source file does not specify required compiler version!");
		SemVerVersion recommendedVersion{string(VersionString)};
		if (!recommendedVersion.isPrerelease())
			errorString +=
				"Consider adding \"pragma solidity ^" +
				to_string(recommendedVersion.major()) +
				string(".") +
				to_string(recommendedVersion.minor()) +
				string(".") +
				to_string(recommendedVersion.patch());
				string(";\"");

		m_errorReporter.warning(_sourceUnit.location(), errorString);
	}
}

bool SyntaxChecker::visit(PragmaDirective const& _pragma)
{
	solAssert(!_pragma.tokens().empty(), "");
	solAssert(_pragma.tokens().size() == _pragma.literals().size(), "");
	if (_pragma.tokens()[0] != Token::Identifier || _pragma.literals()[0] != "solidity")
		m_errorReporter.syntaxError(_pragma.location(), "Unknown pragma \"" + _pragma.literals()[0] + "\"");
	else
	{
		vector<Token::Value> tokens(_pragma.tokens().begin() + 1, _pragma.tokens().end());
		vector<string> literals(_pragma.literals().begin() + 1, _pragma.literals().end());
		SemVerMatchExpressionParser parser(tokens, literals);
		auto matchExpression = parser.parse();
		SemVerVersion currentVersion{string(VersionString)};
		if (!matchExpression.matches(currentVersion))
			m_errorReporter.syntaxError(
				_pragma.location(),
				"Source file requires different compiler version (current compiler is " +
				string(VersionString) + " - note that nightly builds are considered to be "
				"strictly less than the released version"
			);
		m_versionPragmaFound = true;
	}
	return true;
}

bool SyntaxChecker::visit(ModifierDefinition const&)
{
	m_placeholderFound = false;
	return true;
}

void SyntaxChecker::endVisit(ModifierDefinition const& _modifier)
{
	if (!m_placeholderFound)
		m_errorReporter.syntaxError(_modifier.body().location(), "Modifier body does not contain '_'.");
	m_placeholderFound = false;
}

bool SyntaxChecker::visit(WhileStatement const&)
{
	m_inLoopDepth++;
	return true;
}

void SyntaxChecker::endVisit(WhileStatement const&	)
{
	m_inLoopDepth--;
}

bool SyntaxChecker::visit(ForStatement const&)
{
	m_inLoopDepth++;
	return true;
}

void SyntaxChecker::endVisit(ForStatement const&)
{
	m_inLoopDepth--;
}

bool SyntaxChecker::visit(Continue const& _continueStatement)
{
	if (m_inLoopDepth <= 0)
		// we're not in a for/while loop, report syntax error
		m_errorReporter.syntaxError(_continueStatement.location(), "\"continue\" has to be in a \"for\" or \"while\" loop.");
	return true;
}

bool SyntaxChecker::visit(Break const& _breakStatement)
{
	if (m_inLoopDepth <= 0)
		// we're not in a for/while loop, report syntax error
		m_errorReporter.syntaxError(_breakStatement.location(), "\"break\" has to be in a \"for\" or \"while\" loop.");
	return true;
}

bool SyntaxChecker::visit(Throw const& _throwStatement)
{
	m_errorReporter.warning(
		_throwStatement.location(),
		"\"throw\" is deprecated in favour of \"revert()\", \"require()\" and \"assert()\"."
	);

	return true;
}

bool SyntaxChecker::visit(UnaryOperation const& _operation)
{
	if (_operation.getOperator() == Token::Add)
		m_errorReporter.warning(_operation.location(), "Use of unary + is deprecated.");
	return true;
}

bool SyntaxChecker::visit(PlaceholderStatement const&)
{
	m_placeholderFound = true;
	return true;
}

bool SyntaxChecker::visit(FunctionTypeName const& _node)
{
	for (auto const& decl: _node.parameterTypeList()->parameters())
		if (!decl->name().empty())
			m_errorReporter.warning(decl->location(), "Naming function type parameters is deprecated.");

	for (auto const& decl: _node.returnParameterTypeList()->parameters())
		if (!decl->name().empty())
			m_errorReporter.warning(decl->location(), "Naming function type return parameters is deprecated.");

	return true;
}
