#include "pch.h"
#include "ProductInfo.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Environment;

namespace SecondShell::Tests
{
  TEST_CLASS(ProductInfoTests)
  {
  public:
    TEST_METHOD(NameIsSecondShell)
    {
      Assert::AreEqual("SecondShell", ProductInfo::Name());
    }

    TEST_METHOD(VersionIsNotEmpty)
    {
      Assert::IsTrue(std::string_view(ProductInfo::Version()).size() > 0);
    }
  };
}