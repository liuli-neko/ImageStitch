#include <gtest/gtest.h>

#include "../common/parameters.hpp"

namespace Test {

using namespace ImageStitch;

TEST(parametersTest, accessOperation) {
  Parameters testParameters;
  testParameters.SetParam("a", 1);
  EXPECT_EQ(testParameters.GetParam<int>("a", -1), 1);
  testParameters.SetParam("b", "test");
  EXPECT_STREQ(testParameters.GetParam<std::string>("b", "").c_str(), "test");
  testParameters.SetParam("c", true);
  EXPECT_EQ(testParameters.GetParam<bool>("c", false), true);
  testParameters.SetParam("c", false, "this is a boolean");
  auto d = testParameters.GetParam<int>("d", -1);
  LOG(INFO) << "d : " << d;
}

} // namespace Test