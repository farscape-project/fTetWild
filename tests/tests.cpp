////////////////////////////////////////////////////////////////////////////////
#include <catch.hpp>
#include <iostream>
////////////////////////////////////////////////////////////////////////////////

// using namespace floattetwild;

TEST_CASE("test-case", "[suite-name]") {
	REQUIRE(1 == Approx(1).margin(1e-10));
	REQUIRE(1 == Approx(1).margin(1e-10));
}