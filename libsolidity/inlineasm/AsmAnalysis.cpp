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
/**
 * Analyzer part of inline assembly.
 */

#include <libsolidity/inlineasm/AsmAnalysis.h>

#include <libsolidity/inlineasm/AsmData.h>

#include <libsolidity/interface/Exceptions.h>
#include <libsolidity/interface/Utils.h>

#include <boost/range/adaptor/reversed.hpp>

#include <memory>
#include <functional>

using namespace std;
using namespace dev;
using namespace dev::solidity;
using namespace dev::solidity::assembly;


bool Scope::registerLabel(string const& _name)
{
	if (exists(_name))
		return false;
	identifiers[_name] = Label();
	return true;
}

bool Scope::registerVariable(string const& _name)
{
	if (exists(_name))
		return false;
	identifiers[_name] = Variable();
	return true;
}

bool Scope::registerFunction(string const& _name, size_t _arguments, size_t _returns)
{
	if (exists(_name))
		return false;
	identifiers[_name] = Function(_arguments, _returns);
	return true;
}

Scope::Identifier* Scope::lookup(string const& _name)
{
	bool crossedFunctionBoundary = false;
	for (Scope* s = this; s; s = s->superScope)
	{
		auto id = identifiers.find(_name);
		if (id != identifiers.end())
		{
			if (crossedFunctionBoundary && id->second.type() == typeid(Scope::Variable))
				return nullptr;
			else
				return &id->second;
		}

		if (s->functionScope)
			crossedFunctionBoundary = true;
	}
	return nullptr;
}

bool Scope::exists(string const& _name)
{
	if (identifiers.count(_name))
		return true;
	else if (superScope)
		return superScope->exists(_name);
	else
		return false;
}

AsmAnalyzer::AsmAnalyzer(AsmAnalyzer::Scopes& _scopes, ErrorList& _errors):
	m_scopes(_scopes), m_errors(_errors)
{
	// Make the Solidity ErrorTag available to inline assembly
	Scope::Label errorLabel;
	errorLabel.id = Scope::Label::errorLabelId;
	scope(nullptr).identifiers["invalidJumpLabel"] = errorLabel;
	m_currentScope = &scope(nullptr);
}

bool AsmAnalyzer::operator()(assembly::Literal const& _literal)
{
	if (!_literal.isNumber && _literal.value.size() > 32)
	{
		m_errors.push_back(make_shared<Error>(
			Error::Type::TypeError,
			"String literal too long (" + boost::lexical_cast<std::string>(_literal.value.size()) + " > 32)"
		));
		return false;
	}
	return true;
}

bool AsmAnalyzer::operator()(FunctionalInstruction const& _instr)
{
	bool success = true;
	for (auto const& arg: _instr.arguments | boost::adaptors::reversed)
		if (!boost::apply_visitor(*this, arg))
			success = false;
	if (!(*this)(_instr.instruction))
		success = false;
	return success;
}

bool AsmAnalyzer::operator()(Label const& _item)
{
	if (!m_currentScope->registerLabel(_item.name))
	{
		//@TODO secondary location
		m_errors.push_back(make_shared<Error>(
			Error::Type::DeclarationError,
			"Label name " + _item.name + " already taken in this scope.",
			_item.location
		));
		return false;
	}
	return true;
}

bool AsmAnalyzer::operator()(FunctionalAssignment const& _assignment)
{
	return boost::apply_visitor(*this, *_assignment.value);
}

bool AsmAnalyzer::operator()(assembly::VariableDeclaration const& _varDecl)
{
	bool success = boost::apply_visitor(*this, *_varDecl.value);
	if (!registerVariable(_varDecl.name, _varDecl.location, *m_currentScope))
		success = false;
	return success;
}

bool AsmAnalyzer::operator()(assembly::FunctionDefinition const& _funDef)
{
	bool success = true;
	if (!m_currentScope->registerFunction(_funDef.name, _funDef.arguments.size(), _funDef.returns.size()))
	{
		//@TODO secondary location
		m_errors.push_back(make_shared<Error>(
			Error::Type::DeclarationError,
			"Function name " + _funDef.name + " already taken in this scope.",
			_funDef.location
		));
		success = false;
	}
	Scope& body = scope(&_funDef.body);
	body.superScope = m_currentScope;
	body.functionScope = true;
	for (auto const& var: _funDef.arguments + _funDef.returns)
		if (!registerVariable(var, _funDef.location, body))
			success = false;

	(*this)(_funDef.body);

	return success;
}

bool AsmAnalyzer::operator()(assembly::FunctionCall const& _funCall)
{
	bool success = true;
	for (auto const& arg: _funCall.arguments | boost::adaptors::reversed)
		if (!boost::apply_visitor(*this, arg))
			success = false;
	// TODO actually look up the function (can only be done in a second pass)
	// and check that the number of arguments and of returns matches the context
	return success;
}

bool AsmAnalyzer::operator()(Block const& _block)
{
	bool success = true;
	scope(&_block).superScope = m_currentScope;
	m_currentScope = &scope(&_block);

	for (auto const& s: _block.statements)
		if (!boost::apply_visitor(*this, s))
			success = false;

	m_currentScope = m_currentScope->superScope;
	return success;
}

bool AsmAnalyzer::registerVariable(string const& _name, SourceLocation const& _location, Scope& _scope)
{
	if (!_scope.registerVariable(_name))
	{
		//@TODO secondary location
		m_errors.push_back(make_shared<Error>(
			Error::Type::DeclarationError,
			"Variable name " + _name + " already taken in this scope.",
			_location
		));
		return false;
	}
	return true;
}

Scope& AsmAnalyzer::scope(Block const* _block)
{
	auto& scope = m_scopes[_block];
	if (!scope)
		scope = make_shared<Scope>();
	return *scope;
}
