#include "pch.h"
#include "CaptureStore.h"

using namespace Axodox::Infrastructure;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace SecondShell::Actions;
using namespace SecondShell::Agent;
using namespace SecondShell::Protocol;
using namespace std;
using namespace std::filesystem;

namespace SecondShell::Tests
{
  TEST_CLASS(CaptureStoreTests)
  {
    dependency_container _container;
    shared_ptr<CaptureStore> _store = _container.resolve<CaptureStore>();
    path _folder = temp_directory_path() / format("SecondShell.CaptureStoreTests.{}", GetCurrentProcessId());

    static CapturedImage Image()
    {
      return { .Width = 2, .Height = 1, .Format = ImageFormat::Png, .Data = { 1, 2, 3 } };
    }

  public:
    TEST_METHOD_CLEANUP(RemoveFiles)
    {
      remove_all(_folder);
    }

    TEST_METHOD(CapturesGoInlineByDefault)
    {
      auto destination = _store->Decide(nullopt, 0);
      Assert::IsTrue(destination.IsInline);
      Assert::IsFalse(destination.IsFile);

      destination = _store->Decide(CaptureOutput::Both, 0);
      Assert::IsTrue(destination.IsInline && destination.IsFile);
    }

    TEST_METHOD(InlineCapturesBeyondTheLimitGoToFile)
    {
      _store->Configure({ .InlineLimit = 2 });

      Assert::IsTrue(_store->Decide(CaptureOutput::Inline, 1).IsInline);
      auto destination = _store->Decide(CaptureOutput::Inline, 2);
      Assert::IsFalse(destination.IsInline);
      Assert::IsTrue(destination.IsFile && destination.IsOverInlineLimit);

      Assert::IsFalse(_store->Decide(CaptureOutput::File, 2).IsOverInlineLimit, L"captures asked for as files are not over the limit");
    }

    TEST_METHOD(DefaultsComeFromTheSettings)
    {
      _store->Configure({ .Resolution = Size{ 640, 480 }, .Format = ImageFormat::Jpeg });

      CaptureScreenAction toFile;
      _store->ApplyDefaults(toFile, _store->Decide(CaptureOutput::File, 0));
      Assert::AreEqual(640u, toFile.Resolution->Width);
      Assert::IsTrue(toFile.Format == ImageFormat::Jpeg);

      CaptureScreenAction inlined;
      inlined.Format = ImageFormat::Jpeg;
      _store->ApplyDefaults(inlined, _store->Decide(nullopt, 0));
      Assert::IsTrue(inlined.Format == ImageFormat::Png, L"inline captures are always PNG");

      ClipboardReadAction read;
      read.Resolution = Size{ 100, 100 };
      _store->ApplyDefaults(read);
      Assert::AreEqual(100u, read.Resolution->Width, L"the action's own resolution wins");
    }

    TEST_METHOD(FilesAreSavedAndTheirDataDropped)
    {
      auto file = _folder / "capture.png";
      auto image = Image();
      ActionResult result;
      _store->Store(image, result, winrt::to_string(file.wstring()), _store->Decide(CaptureOutput::File, 0), "unused");

      Assert::IsTrue(exists(file));
      Assert::AreEqual(uintmax_t(3), file_size(file));
      Assert::AreEqual(winrt::to_string(file.wstring()), *image.Path);
      Assert::IsTrue(image.Data.empty(), L"a capture only to file returns no data");
    }

    TEST_METHOD(CapturesOverTheLimitSayWhy)
    {
      _store->Configure({ .InlineLimit = 0 });

      auto image = Image();
      ActionResult result;
      _store->Store(image, result, winrt::to_string((_folder / "over.png").wstring()), _store->Decide(nullopt, 0), "unused");
      Assert::IsTrue(result.Message->find("most inline captures") != string::npos, to_wstring(*result.Message).c_str());
    }
  };
}
