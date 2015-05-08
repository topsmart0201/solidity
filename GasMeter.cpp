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
 * @date 2015
 * Unit tests for the gas estimator.
 */

#include <test/libsolidity/solidityExecutionFramework.h>
#include <libsolidity/AST.h>
#include <libsolidity/StructuralGasEstimator.h>
#include <libsolidity/SourceReferenceFormatter.h>

using namespace std;
using namespace dev::eth;
using namespace dev::solidity;

namespace dev
{
namespace solidity
{
namespace test
{

class GasMeterTestFramework: public ExecutionFramework
{
public:
	GasMeterTestFramework() { }
	void compile(string const& _sourceCode)
	{
		m_compiler.setSource(_sourceCode);
		ETH_TEST_REQUIRE_NO_THROW(m_compiler.compile(), "Compiling contract failed");

		StructuralGasEstimator estimator;
		AssemblyItems const* items = m_compiler.getRuntimeAssemblyItems("");
		ASTNode const& sourceUnit = m_compiler.getAST();
		BOOST_REQUIRE(items != nullptr);
		m_gasCosts = estimator.breakToStatementLevel(
			estimator.performEstimation(*items, vector<ASTNode const*>({&sourceUnit})),
			{&sourceUnit}
		);
	}

protected:
	dev::solidity::CompilerStack m_compiler;
	map<ASTNode const*, eth::GasMeter::GasConsumption> m_gasCosts;
};

BOOST_FIXTURE_TEST_SUITE(GasMeterTests, GasMeterTestFramework)

BOOST_AUTO_TEST_CASE(non_overlapping_filtered_costs)
{
	char const* sourceCode = R"(
		contract test {
			bytes x;
			function f(uint a) returns (uint b) {
				x.length = a;
				for (; a < 200; ++a) {
					x[a] = 9;
					b = a * a;
				}
				return f(a - 1);
			}
		}
	)";
	compile(sourceCode);
	for (auto first = m_gasCosts.cbegin(); first != m_gasCosts.cend(); ++first)
	{
		auto second = first;
		for (++second; second != m_gasCosts.cend(); ++second)
			if (first->first->getLocation().intersects(second->first->getLocation()))
			{
				BOOST_CHECK_MESSAGE(false, "Source locations should not overlap!");
				SourceReferenceFormatter::printSourceLocation(cout, first->first->getLocation(), m_compiler.getScanner());
				SourceReferenceFormatter::printSourceLocation(cout, second->first->getLocation(), m_compiler.getScanner());
			}
	}
}

BOOST_AUTO_TEST_SUITE_END()

}
}
}
