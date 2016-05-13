#define CATCH_CONFIG_MAIN
#include <iostream>
#include <cstdlib>
#include <string>
#include "utils.hpp"
#include "catch.hpp"

using namespace std;
using namespace ASIOLibs;

TEST_CASE( "ScopeGuard", "[utils]" ) {
	int val=0;
	{
		ScopeGuard sg([&val]() {
			val=1;
		});
	}
	REQUIRE( val == 1 );
	val=0;
	{
		ScopeGuard sg([&val]() {
			val=1;
		});
		sg.Forget();
	}
	REQUIRE( val == 0 );
}

TEST_CASE( "bin2hex & hex2bin", "[utils]" ) {
	srand( getpid() ^ time(NULL) );
	enum { test_sz=100 };
	std::string bin;
	Optional<std::string> bin2;
	std::string hex;
	for(int l=0; l<10; l++)	{
		bin.resize(test_sz);
		for(int i=0; i<test_sz; i++) {
			bin[i] = l==0 ? i : rand()%255;
		}
		hex = bin2hex(bin);
		bin2 = hex2bin(hex);
		REQUIRE( bin2.hasValue() == true );
		REQUIRE( bin == *bin2 );
	}

	bin2 = hex2bin("xx");
	REQUIRE( bin2.hasValue() == false );
}

TEST_CASE( "ForceFree", "[utils]" ) {
	string t0;
	t0.reserve(10);
	REQUIRE( t0.capacity() >= 10 );
	t0.reserve(50);
	REQUIRE( t0.capacity() >= 50 );
	REQUIRE( t0.capacity() < 500 );
	t0.reserve(500);
	REQUIRE( t0.capacity() >= 500 );
	ForceFree(t0);
	REQUIRE( t0.capacity() < 500 );
}

TEST_CASE( "StrFormatter", "[utils]" ) {
	StrFormatter s;
	s << "abc" << 123U;
	REQUIRE( s.str() == "abc123" );
	REQUIRE( static_cast<std::string>(s) == "abc123" );
}

TEST_CASE( "Optional", "[utils]" ) {
	Optional<int> o(123);
	REQUIRE( o.hasValue() == true );
	REQUIRE( *o == 123 );
	Optional<int> o2;
	REQUIRE( o2.hasValue() == false );
}

#define VERY_LONG_STR "bugsojpcqmgbtigpwulvrngjysihnitnfttrbngfujttkmwoysyjaqatugugafbmbfyokpusdskcazydvyareytjicjfjcvpqdmoiyxchmvyo" \
"vmcddalpvxbskabrfwyonugzmnhnpwsmtdwzayyrqamaxmckqfcuwtxybhlbfzdljqwnxekiudgcvfzersvkwasrqupfsbavvpwliefoiizfkrsjkllqestwdumypwcootzbir" \
"fyxnipywoqgwiogwnrxdohcfmftxljwcfnfyvyipuuuocbniazyrcikcrxpcznxjulwektbtlsiylslfeyvydrxtfikjcpnvsxqcbmupwtzmxsvudbixwwwmfmjdxqstacjazvh" \
"ufvfxsvigrmsfwefzfwqxtrkvyfaubsalanczshvqqopqsfrjbtmknpwfwqldoeifimayyylqkzvhfdcbckxlxuntqcgeigqbetevnaxshxqoikdcipqexdfncxrwjrxcpxgshkxxgrvdt"
TEST_CASE( "string_sprintf", "[utils]" ) {
	REQUIRE( string_sprintf("test%u", 123) == "test123" );
	REQUIRE( string_sprintf("test%s", "test") == "testtest" );
	REQUIRE( string_sprintf("test%zu", 123) == "test123" );
	REQUIRE( string_sprintf("test%lu", 123) == "test123" );
	REQUIRE( string_sprintf("test%d", 123) == "test123" );
	REQUIRE( string_sprintf("test%s", VERY_LONG_STR) == "test" VERY_LONG_STR );
}
