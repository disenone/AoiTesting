// Copyright <disenone>

#include <iostream>

#define BOOST_TEST_MODULE test_squares
#define BOOST_TEST_DYN_LINK
#include <boost/test/included/unit_test.hpp>

#include <common/nuid.hpp>

using namespace aoi;

BOOST_AUTO_TEST_CASE(test_nuid) {
  Nuid nuid1 = aoi::GenNuid();
  Nuid nuid2 = aoi::GenNuid();
  Nuid nuid3 = aoi::GenNuid();
  BOOST_TEST(nuid2 == nuid1 + 1);
  BOOST_TEST(nuid3 == nuid2 + 1);
}
