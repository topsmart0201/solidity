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
/** @file trie.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 * Trie test functions.
 */

#include <fstream>
#include <random>
#include "JsonSpiritHeaders.h"
#include <libethcore/TrieDB.h>
#include "TrieHash.h"
#include "MemTrie.h"
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace eth;

namespace js = json_spirit;

namespace eth 
{ 
	namespace test
	{
		static unsigned fac(unsigned _i) 
		{ 
			return _i > 2 ? _i * fac(_i - 1) : _i; 
		}

	}
}


BOOST_AUTO_TEST_CASE(trie_tests)
{
	cnote << "Testing Trie...";

	js::mValue v;
	string s = asString(contents("../../tests/trietest.json"));
	BOOST_REQUIRE_MESSAGE( s.length() > 0, "Contents of 'trietest.json' is empty. Have you cloned the 'tests' repo branch develop?");
	js::read_string(s, v);
	for (auto& i: v.get_obj())
	{
		js::mObject& o = i.second.get_obj();
		cnote << i.first;
		vector<pair<string, string>> ss;
		for (auto& i: o["in"].get_obj())
			ss.push_back(make_pair(i.first, i.second.get_str()));
		for (unsigned j = 0; j < eth::test::fac((unsigned)ss.size()); ++j)
		{
			next_permutation(ss.begin(), ss.end());
			BasicMap m;
			GenericTrieDB<BasicMap> t(&m);
			t.init();
			for (auto const& k: ss)
				t.insert(k.first, k.second);
			BOOST_REQUIRE(!o["root"].is_null()); 
			BOOST_CHECK(o["root"].get_str() == toHex(t.root().asArray()) ); 
		}
	}
}


inline h256 stringMapHash256(StringMap const& _s)
{
	return hash256(_s);
}

int trieTest()
{

	// More tests...
	{
		BasicMap m;
		GenericTrieDB<BasicMap> t(&m);
		t.init();	// initialise as empty tree.
		cout << t;
		cout << m;
		cout << t.root() << endl;
		cout << hash256(StringMap()) << endl;

		t.insert(string("tesz"), string("test"));
		cout << t;
		cout << m;
		cout << t.root() << endl;
		cout << stringMapHash256({{"test", "test"}}) << endl;

		t.insert(string("tesa"), string("testy"));
		cout << t;
		cout << m;
		cout << t.root() << endl;
		cout << stringMapHash256({{"test", "test"}, {"te", "testy"}}) << endl;
		cout << t.at(string("test")) << endl;
		cout << t.at(string("te")) << endl;
		cout << t.at(string("t")) << endl;

		t.remove(string("te"));
		cout << m;
		cout << t.root() << endl;
		cout << stringMapHash256({{"test", "test"}}) << endl;

		t.remove(string("test"));
		cout << m;
		cout << t.root() << endl;
		cout << hash256(StringMap()) << endl;
	}
	{
		BasicMap m;
		GenericTrieDB<BasicMap> t(&m);
		t.init();	// initialise as empty tree.
		t.insert(string("a"), string("A"));
		t.insert(string("b"), string("B"));
		cout << t;
		cout << m;
		cout << t.root() << endl;
		cout << stringMapHash256({{"b", "B"}, {"a", "A"}}) << endl;
		cout << RLP(rlp256({{"b", "B"}, {"a", "A"}})) << endl;
	}
	{
		MemTrie t;
		t.insert("dog", "puppy");
		cout << hex << t.hash256() << endl;
		cout << RLP(t.rlp()) << endl;
	}
	{
		MemTrie t;
		t.insert("bed", "d");
		t.insert("be", "e");
		cout << hex << t.hash256() << endl;
		cout << RLP(t.rlp()) << endl;
	}
	{
		cout << hex << stringMapHash256({{"dog", "puppy"}, {"doe", "reindeer"}}) << endl;
		MemTrie t;
		t.insert("dog", "puppy");
		t.insert("doe", "reindeer");
		cout << hex << t.hash256() << endl;
		cout << RLP(t.rlp()) << endl;
		cout << toHex(t.rlp()) << endl;
	}
	{
		BasicMap m;
		GenericTrieDB<BasicMap> d(&m);
		d.init();	// initialise as empty tree.
		MemTrie t;
		StringMap s;

		auto add = [&](char const* a, char const* b)
		{
			d.insert(string(a), string(b));
			t.insert(a, b);
			s[a] = b;

			cout << endl << "-------------------------------" << endl;
			cout << a << " -> " << b << endl;
			cout << d;
			cout << m;
			cout << d.root() << endl;
			cout << hash256(s) << endl;

			assert(t.hash256() == hash256(s));
			assert(d.root() == hash256(s));
			for (auto const& i: s)
			{
				(void)i;
				assert(t.at(i.first) == i.second);
				assert(d.at(i.first) == i.second);
			}
		};

		auto remove = [&](char const* a)
		{
			s.erase(a);
			t.remove(a);
			d.remove(string(a));

			cout << endl << "-------------------------------" << endl;
			cout << "X " << a << endl;
			cout << d;
			cout << m;
			cout << d.root() << endl;
			cout << hash256(s) << endl;

			assert(t.at(a).empty());
			assert(d.at(string(a)).empty());
			assert(t.hash256() == hash256(s));
			assert(d.root() == hash256(s));
			for (auto const& i: s)
			{
				(void)i;
				assert(t.at(i.first) == i.second);
				assert(d.at(i.first) == i.second);
			}
		};

		add("dogglesworth", "cat");
		add("doe", "reindeer");
		remove("dogglesworth");
		add("horse", "stallion");
		add("do", "verb");
		add("doge", "coin");
		remove("horse");
		remove("do");
		remove("doge");
		remove("doe");
		for (int a = 0; a < 20; ++a)
		{
			StringMap m;
			for (int i = 0; i < 20; ++i)
			{
				auto k = randomWord();
				auto v = toString(i);
				m.insert(make_pair(k, v));
				t.insert(k, v);
				d.insert(k, v);
				assert(hash256(m) == t.hash256());
				assert(hash256(m) == d.root());
			}
			while (!m.empty())
			{
				auto k = m.begin()->first;
				d.remove(k);
				t.remove(k);
				m.erase(k);
				assert(hash256(m) == t.hash256());
				assert(hash256(m) == d.root());
			}
		}
	}
	return 0;
}

