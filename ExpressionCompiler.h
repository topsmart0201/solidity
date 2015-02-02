/*
	This file is part of cpp-ethereum.

	cpp-ethereum is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	cpp-ethereum is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
 * @author Christian <c@ethdev.com>
 * @author Gav Wood <g@ethdev.com>
 * @date 2014
 * Solidity AST to EVM bytecode compiler for expressions.
 */

#include <functional>
#include <boost/noncopyable.hpp>
#include <libdevcore/Common.h>
#include <libsolidity/ASTVisitor.h>

namespace dev {
namespace eth
{
class AssemblyItem; // forward
}
namespace solidity {

// forward declarations
class CompilerContext;
class Type;
class IntegerType;
class StaticStringType;

/**
 * Compiler for expressions, i.e. converts an AST tree whose root is an Expression into a stream
 * of EVM instructions. It needs a compiler context that is the same for the whole compilation
 * unit.
 */
class ExpressionCompiler: private ASTConstVisitor
{
public:
	/// Compile the given @a _expression into the @a _context.
	static void compileExpression(CompilerContext& _context, Expression const& _expression, bool _optimize = false);

	/// Appends code to remove dirty higher order bits in case of an implicit promotion to a wider type.
	static void appendTypeConversion(CompilerContext& _context, Type const& _typeOnStack,
									 Type const& _targetType, bool _cleanupNeeded = false);
	/// Appends code for a State Variable accessor function
	static void appendStateVariableAccessor(CompilerContext& _context, VariableDeclaration const& _varDecl, bool _optimize = false);

private:
	explicit ExpressionCompiler(CompilerContext& _compilerContext, bool _optimize = false):
		m_optimize(_optimize), m_context(_compilerContext), m_currentLValue(m_context) {}

	virtual bool visit(Assignment const& _assignment) override;
	virtual bool visit(UnaryOperation const& _unaryOperation) override;
	virtual bool visit(BinaryOperation const& _binaryOperation) override;
	virtual bool visit(FunctionCall const& _functionCall) override;
	virtual bool visit(NewExpression const& _newExpression) override;
	virtual void endVisit(MemberAccess const& _memberAccess) override;
	virtual bool visit(IndexAccess const& _indexAccess) override;
	virtual void endVisit(Identifier const& _identifier) override;
	virtual void endVisit(Literal const& _literal) override;

	///@{
	///@name Append code for various operator types
	void appendAndOrOperatorCode(BinaryOperation const& _binaryOperation);
	void appendCompareOperatorCode(Token::Value _operator, Type const& _type);
	void appendOrdinaryBinaryOperatorCode(Token::Value _operator, Type const& _type);

	void appendArithmeticOperatorCode(Token::Value _operator, Type const& _type);
	void appendBitOperatorCode(Token::Value _operator);
	void appendShiftOperatorCode(Token::Value _operator);
	/// @}

	/// Appends an implicit or explicit type conversion. For now this comprises only erasing
	/// higher-order bits (@see appendHighBitCleanup) when widening integer.
	/// If @a _cleanupNeeded, high order bits cleanup is also done if no type conversion would be
	/// necessary.
	void appendTypeConversion(Type const& _typeOnStack, Type const& _targetType, bool _cleanupNeeded = false);
	//// Appends code that cleans higher-order bits for integer types.
	void appendHighBitsCleanup(IntegerType const& _typeOnStack);

	/// Appends code to call a function of the given type with the given arguments.
	void appendExternalFunctionCall(FunctionType const& _functionType, std::vector<ASTPointer<Expression const>> const& _arguments,
									bool bare = false);
	/// Appends code that copies the given arguments to memory (with optional offset).
	/// @returns the number of bytes copied to memory
	unsigned appendArgumentCopyToMemory(TypePointers const& _types,
										std::vector<ASTPointer<Expression const>> const& _arguments,
										unsigned _memoryOffset = 0);
	/// Appends code that copies a type to memory.
	/// @returns the number of bytes copied to memory
	unsigned appendTypeConversionAndMoveToMemory(Type const& _expectedType, Type const& _type,
												 Location const& _location, unsigned _memoryOffset = 0);
	/// Appends code that evaluates a single expression and copies it to memory (with optional offset).
	/// @returns the number of bytes copied to memory
	unsigned appendExpressionCopyToMemory(Type const& _expectedType, Expression const& _expression,
										  unsigned _memoryOffset = 0);

	/// Appends code for a State Variable accessor function
	void appendStateVariableAccessor(VariableDeclaration const& _varDecl);

	/**
	 * Helper class to store and retrieve lvalues to and from various locations.
	 * All types except STACK store a reference in a slot on the stack, STACK just
	 * stores the base stack offset of the variable in @a m_baseStackOffset.
	 */
	class LValue
	{
	public:
		enum LValueType { NONE, STACK, MEMORY, STORAGE };

		explicit LValue(CompilerContext& _compilerContext): m_context(&_compilerContext) { reset(); }
		LValue(CompilerContext& _compilerContext, LValueType _type, Type const& _dataType, unsigned _baseStackOffset = 0);

		/// Set type according to the declaration and retrieve the reference.
		/// @a _expression is the current expression
		void fromIdentifier(Identifier const& _identifier, Declaration const& _declaration);
		/// Convenience function to set type for a state variable and retrieve the reference
		void fromStateVariable(Declaration const& _varDecl, TypePointer const& _type);
		void reset() { m_type = NONE; m_baseStackOffset = 0; m_size = 0; }

		bool isValid() const { return m_type != NONE; }
		bool isInOnStack() const { return m_type == STACK; }
		bool isInMemory() const { return m_type == MEMORY; }
		bool isInStorage() const { return m_type == STORAGE; }

		/// @returns true if this lvalue reference type occupies a slot on the stack.
		bool storesReferenceOnStack() const { return m_type == STORAGE || m_type == MEMORY; }

		/// Copies the value of the current lvalue to the top of the stack and, if @a _remove is true,
		/// also removes the reference from the stack (note that is does not reset the type to @a NONE).
		/// @a _type is the type of the current expression and @ _location its location, used for error reporting.
		/// @a _location can be a nullptr for expressions that don't have an actual ASTNode equivalent
		void retrieveValue(TypePointer const& _type, Location const& _location, bool _remove = false) const;
		/// Stores a value (from the stack directly beneath the reference, which is assumed to
		/// be on the top of the stack, if any) in the lvalue and removes the reference.
		/// Also removes the stored value from the stack if @a _move is
		/// true. @a _expression is the current expression, used for error reporting.
		void storeValue(Expression const& _expression, bool _move = false) const;
		/// Stores zero in the lvalue.
		/// @a _expression is the current expression, used for error reporting.
		void setToZero(Expression const& _expression) const;
		/// Convenience function to convert the stored reference to a value and reset type to NONE if
		/// the reference was not requested by @a _expression.
		void retrieveValueIfLValueNotRequested(Expression const& _expression);

	private:
		/// Convenience function to retrieve Value from Storage. Specific version of @ref retrieveValue
		void retrieveValueFromStorage(TypePointer const& _type, bool _remove = false) const;

		CompilerContext* m_context;
		LValueType m_type = NONE;
		/// If m_type is STACK, this is base stack offset (@see
		/// CompilerContext::getBaseStackOffsetOfVariable) of a local variable.
		unsigned m_baseStackOffset = 0;
		/// Size of the value of this lvalue on the stack or the storage.
		unsigned m_size = 0;
	};

	bool m_optimize;
	CompilerContext& m_context;
	LValue m_currentLValue;
};


}
}
