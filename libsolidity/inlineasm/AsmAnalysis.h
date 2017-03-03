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
 * Analysis part of inline assembly.
 */

#pragma once

#include <libsolidity/interface/Exceptions.h>

#include <boost/variant.hpp>

#include <functional>
#include <memory>

namespace dev
{
namespace solidity
{
namespace assembly
{

struct Literal;
struct Block;
struct Label;
struct FunctionalInstruction;
struct FunctionalAssignment;
struct VariableDeclaration;
struct Instruction;
struct Identifier;
struct Assignment;

struct Scope
{
	struct Variable
	{
		int stackHeight = 0;
		bool active = false;
	};

	struct Label
	{
		Label(size_t _id): id(_id) {}
		size_t id = 0;
		int stackAdjustment = 0;
		bool resetStackHeight = false;
		static const size_t errorLabelId = -1;
		static const size_t unassignedLabelId = 0;
	};

	using Identifier = boost::variant<Variable, Label>;

	struct Visitor: public boost::static_visitor<>
	{
		Visitor(
			std::function<void(Variable const&)> _varVisitor,
			std::function<void(Label const&)> _labelVisitor
		):
			m_varVisitor(std::move(_varVisitor)),
			m_labelVisitor(std::move(_labelVisitor))
		{}

		void operator()(Variable const& _var) const { m_varVisitor(_var); }
		void operator()(Label const& _label) const { m_labelVisitor(_label); }

		std::function<void(Variable const&)> m_varVisitor;
		std::function<void(Label const&)> m_labelVisitor;
	};
	struct NonconstVisitor: public boost::static_visitor<>
	{
		NonconstVisitor(
			std::function<void(Variable&)> _varVisitor,
			std::function<void(Label&)> _labelVisitor
		):
			m_varVisitor(std::move(_varVisitor)),
			m_labelVisitor(std::move(_labelVisitor))
		{}

		void operator()(Variable& _var) const { m_varVisitor(_var); }
		void operator()(Label& _label) const { m_labelVisitor(_label); }

		std::function<void(Variable&)> m_varVisitor;
		std::function<void(Label&)> m_labelVisitor;
	};

	bool registerLabel(std::string const& _name, size_t _id);

	bool registerVariable(std::string const& _name);

	/// Looks up the identifier in this or super scopes and returns a valid pointer if
	/// found or a nullptr if not found.
	/// The pointer will be invalidated if the scope is modified.
	Identifier* lookup(std::string const& _name);
	/// Looks up the identifier in this and super scopes and calls the visitor, returns false if not found.
	template <class V>
	bool lookup(std::string const& _name, V const& _visitor)
	{
		if (Identifier* id = lookup(_name))
		{
			boost::apply_visitor(_visitor, *id);
			return true;
		}
		else
			return false;
	}



	Scope* superScope = nullptr;
	std::map<std::string, Identifier> identifiers;
};


class AsmAnalyzer: public boost::static_visitor<bool>
{
public:
	using Scopes = std::map<assembly::Block const*, std::shared_ptr<Scope>>;
	AsmAnalyzer(Scopes& _scopes, ErrorList& _errors);

	bool operator()(assembly::Instruction const&) { return true; }
	bool operator()(assembly::Literal const& _literal);
	bool operator()(assembly::Identifier const&) { return true; }
	bool operator()(assembly::FunctionalInstruction const& _functionalInstruction);
	bool operator()(assembly::Label const& _label);
	bool operator()(assembly::Assignment const&) { return true; }
	bool operator()(assembly::FunctionalAssignment const& _functionalAssignment);
	bool operator()(assembly::VariableDeclaration const& _variableDeclaration);
	bool operator()(assembly::Block const& _block);

private:
	Scope* m_currentScope = nullptr;
	Scopes& m_scopes;
	ErrorList& m_errors;
};

}
}
}
