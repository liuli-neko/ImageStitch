#include <gtest/gtest.h>

#include "../common/parameters.hpp"

namespace Test {

using namespace ImageStitch;

TEST(parametersTest, accessOperation) {
  Parameters testParameters;
  testParameters.SetParam("a", 1);
  EXPECT_EQ(testParameters.GetParam<int>("a", -1), 1);
  testParameters.SetParam("a", 1.0);
  EXPECT_DOUBLE_EQ(testParameters.GetParam<double>("a", -1.0), 1.0);
  testParameters.SetParam("b", "test");
  EXPECT_STREQ(testParameters.GetParam<std::string>("b", "").c_str(), "test");
  testParameters.SetParam("c", true);
  EXPECT_EQ(testParameters.GetParam<bool>("c", false), true);
  testParameters.SetParam("c", false, "this is a boolean");
  auto d = testParameters.GetParam<int>("d", -1);
  LOG(INFO) << "d : " << d;
  testParameters.Save("./test.json");
  Parameters testParameters1;
  testParameters1.Load("./test.json");
  EXPECT_EQ(testParameters.GetParam<int>("a", -1),
            testParameters1.GetParam<int>("a", -2));
  EXPECT_STREQ(testParameters.GetParam<std::string>("b", "").c_str(),
               testParameters1.GetParam<std::string>("b", "1").c_str());
  EXPECT_EQ(testParameters.GetParam<bool>("c", false),
            testParameters.GetParam<bool>("c", false));
}

}  // namespace Test