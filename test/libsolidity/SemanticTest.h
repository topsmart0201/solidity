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

#pragma once

#include <test/libsolidity/util/TestFileParser.h>
#include <test/libsolidity/FormattedScope.h>
#include <test/libsolidity/SolidityExecutionFramework.h>
#include <test/libsolidity/AnalysisFramework.h>
#include <test/TestCase.h>
#include <liblangutil/Exceptions.h>

#include <iosfwd>
#include <string>
#include <vector>
#include <utility>

namespace dev
{
namespace solidity
{
namespace test
{

/**
 * Class that represents a semantic test (or end-to-end test) and allows running it as part of the
 * boost unit test environment or isoltest. It reads the Solidity source and an additional comment
 * section from the given file. This comment section should define a set of functions to be called
 * and an expected result they return after being executed.
 */
class SemanticTest: public SolidityExecutionFramework, public TestCase
{
public:
	/**
	 * Represents a function call and the result it returned. It stores the call
	 * representation itself, the actual byte result (if any) and a string representation
	 * used for the interactive update routine provided by isoltest. It also provides
	 * functionality to compare the actual result with the expectations attached to the
	 * call object, as well as a way to reset the result if executed multiple times.
	 */
	struct FunctionCallTest
	{
		FunctionCall call;
		bytes rawBytes;
		std::string output;
		bool failure = true;
		/// Compares raw expectations (which are converted to a byte representation before),
		/// and also the expected transaction status of the function call to the actual test results.
		bool matchesExpectation() const
		{
			return failure == call.expectations.failure && rawBytes == call.expectations.rawBytes();
		}
		/// Resets current results in case the function was called and the result
		/// stored already (e.g. if test case was updated via isoltest).
		void reset()
		{
			failure = true;
			rawBytes = bytes{};
			output = std::string{};
		}
	};

	static std::unique_ptr<TestCase> create(Config const& _options)
	{ return std::make_unique<SemanticTest>(_options.filename, _options.ipcPath); }

	explicit SemanticTest(std::string const& _filename, std::string const& _ipcPath);

	bool run(std::ostream& _stream, std::string const& _linePrefix = "", bool const _formatted = false) override;
	void printSource(std::ostream &_stream, std::string const& _linePrefix = "", bool const _formatted = false) const override;
	void printUpdatedExpectations(std::ostream& _stream, std::string const& _linePrefix = "") const override;

	/// Instantiates a test file parser that parses the additional comment section at the end of
	/// the input stream \param _stream. Each function call is represented using a `FunctionCallTest`
	/// and added to the list of call to be executed when `run()` is called.
	/// Throws if parsing expectations failed.
	void parseExpectations(std::istream& _stream);

	/// Compiles and deploys currently held source.
	/// Returns true if deployment was successful, false otherwise.
	bool deploy(std::string const& _contractName, u256 const& _value, bytes const& _arguments);

	std::string m_source;
	std::vector<FunctionCallTest> m_tests;
};

}
}
}
