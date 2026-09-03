# Google Test

**Version:** v1.15.2
**Source:** https://github.com/google/googletest (tag v1.15.2)
**License:** BSD-3-Clause (see googletest/LICENSE)

The upstream `googletest/` tree is committed unmodified. `build/gtest.vcxproj`
is our wrapper — it builds `gtest-all.cc` + `gtest_main.cc` as a single static lib
called `gtest.lib` for each of the four (Debug/Release × Win32/x64) configurations.

Consuming test projects add `..\..\Common\include\googletest\googletest\include`
to their include path and link against `$(OutDir)gtest.lib`.
