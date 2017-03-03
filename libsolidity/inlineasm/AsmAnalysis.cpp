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


bool Scope::registerLabel(const string& _name, size_t _id)
{
	if (lookup(_name))
		return false;
	identifiers[_name] = Scope::Label(_id);
	return true;
}


bool Scope::registerVariable(const string& _name)
{
	if (lookup(_name))
		return false;
	identifiers[_name] = Variable();
	return true;
}

Scope::Identifier* Scope::lookup(string const& _name)
{
	if (identifiers.count(_name))
		return &identifiers[_name];
	else if (superScope)
		return superScope->lookup(_name);
	else
		return nullptr;
}

AsmAnalyzer::AsmAnalyzer(AsmAnalyzer::Scopes& _scopes, ErrorList& _errors):
	m_scopes(_scopes), m_errors(_errors)
{
	// Make the Solidity ErrorTag available to inline assembly
	m_scopes[nullptr] = make_shared<Scope>();
	m_scopes[nullptr]->identifiers["invalidJumpLabel"] = Scope::Label(Scope::Label::errorLabelId);
	m_currentScope = m_scopes[nullptr].get();
}

bool AsmAnalyzer::operator()(assembly::Literal const& _literal)
{
	if (!_literal.isNumber && _literal.value.size() > 32)
	{
		m_errors.push_back(make_shared<Error>(
			Error::Type::TypeError,
			"string literal too long (" + boost::lexical_cast<std::string>(_literal.value.size()) + " > 32)"
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
	if (!m_currentScope->registerLabel(_item.name, Scope::Label::unassignedLabelId))
	{
		//@TODO secondary location
		m_errors.push_back(make_shared<Error>(
			Error::Type::DeclarationError,
			"Label name " + _item.name + " already taken in this scope.",
			_item.location
		));
		return false;
	}
	bool success = true;
	if (!_item.stackInfo.empty())
	{
		Scope::Label& label = boost::get<Scope::Label>(m_currentScope->identifiers[_item.name]);
		if (_item.stackInfo.size() == 1)
			try
			{
				label.stackAdjustment = boost::lexical_cast<int>(_item.stackInfo[0]);
				label.resetStackHeight = false;
				return true;
			}
			catch (boost::bad_lexical_cast const&)
			{
				// Interpret as variable name
			}
		label.resetStackHeight = true;
		for (auto const& stackItem: _item.stackInfo)
		{
			if (!m_currentScope->registerVariable(stackItem))
			{
				//@TODO secondary location
				m_errors.push_back(make_shared<Error>(
					Error::Type::DeclarationError,
					"Variable name " + stackItem + " already taken in this scope.",
					_item.location
				));
				success = false;
			}
		}
	}
	return success;
}

bool AsmAnalyzer::operator()(const FunctionalAssignment& _assignment)
{
	return boost::apply_visitor(*this, *_assignment.value);
}

bool AsmAnalyzer::operator()(assembly::VariableDeclaration const& _varDecl)
{
	bool success = boost::apply_visitor(*this, *_varDecl.value);
	if (!m_currentScope->registerVariable(_varDecl.name))
	{
		//@TODO secondary location
		m_errors.push_back(make_shared<Error>(
			Error::Type::DeclarationError,
			"Variable name " + _varDecl.name + " already taken in this scope.",
			_varDecl.location
		));
		success = false;
	}
	return success;
}

bool AsmAnalyzer::operator()(Block const& _block)
{
	bool success = true;
	auto scope = make_shared<Scope>();
	scope->superScope = m_currentScope;
	m_scopes[&_block] = scope;
	m_currentScope = scope.get();

	for (auto const& s: _block.statements)
		if (!boost::apply_visitor(*this, s))
			success = false;

	m_currentScope = m_currentScope->superScope;
	return success;
}
