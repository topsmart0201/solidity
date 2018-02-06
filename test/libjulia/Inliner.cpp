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
 * @date 2017
 * Unit tests for the iulia function inliner.
 */

#include <test/libjulia/Common.h>

#include <libjulia/optimiser/InlinableFunctionFilter.h>
#include <libjulia/optimiser/FunctionalInliner.h>

#include <boost/test/unit_test.hpp>

#include <boost/range/adaptors.hpp>
#include <boost/algorithm/string/join.hpp>

using namespace std;
using namespace dev;
using namespace dev::julia;
using namespace dev::julia::test;
using namespace dev::solidity::assembly;

namespace
{
string inlinableFunctions(string const& _source)
{
	auto ast = disambiguate(_source);

	InlinableFunctionFilter filter;
	filter(*ast);

	return boost::algorithm::join(
		filter.inlinableFunctions() | boost::adaptors::map_keys,
		","
	);
}

}

BOOST_AUTO_TEST_SUITE(IuliaInliner)

BOOST_AUTO_TEST_CASE(smoke_test)
{
	BOOST_CHECK_EQUAL(inlinableFunctions("{ }"), "");
}

BOOST_AUTO_TEST_CASE(simple)
{
	BOOST_CHECK_EQUAL(inlinableFunctions("{ function f() -> x:u256 { x := 2:u256 } }"), "f");
	BOOST_CHECK_EQUAL(inlinableFunctions(R"({
		function g(a:u256) -> b:u256 { b := a }
		function f() -> x:u256 { x := g(2:u256) }
	})"), "f,g");
}

BOOST_AUTO_TEST_CASE(simple_inside_structures)
{
	BOOST_CHECK_EQUAL(inlinableFunctions(R"({
		switch 2:u256
		case 2:u256 {
			function g(a:u256) -> b:u256 { b := a }
			function f() -> x:u256 { x := g(2:u256) }
		}
	})"), "f,g");
	BOOST_CHECK_EQUAL(inlinableFunctions(R"({
		for {
			function g(a:u256) -> b:u256 { b := a }
		} 1:u256 {
			function f() -> x:u256 { x := g(2:u256) }
		}
		{
			function h() -> y:u256 { y := 2:u256 }
		}
	})"), "f,g,h");
}

BOOST_AUTO_TEST_CASE(negative)
{
	BOOST_CHECK_EQUAL(inlinableFunctions("{ function f() -> x:u256 { } }"), "");
	BOOST_CHECK_EQUAL(inlinableFunctions("{ function f() -> x:u256 { x := 2:u256 {} } }"), "");
	BOOST_CHECK_EQUAL(inlinableFunctions("{ function f() -> x:u256 { x := f() } }"), "");
	BOOST_CHECK_EQUAL(inlinableFunctions("{ function f() -> x:u256 { x := x } }"), "");
	BOOST_CHECK_EQUAL(inlinableFunctions("{ function f() -> x:u256, y:u256 { x := 2:u256 } }"), "");
}


BOOST_AUTO_TEST_SUITE_END()
