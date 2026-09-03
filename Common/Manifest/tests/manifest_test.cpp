#include <gtest/gtest.h>
#include "../Manifest.h"

using namespace hac::manifest;

TEST(ManifestLoad, ValidManifestParsesGameFields) {
	Manifest m;
	ASSERT_EQ(m.Load(L"fixtures/valid.manifest"), ManifestError::Ok);
	EXPECT_EQ(m.Game().id, "cstrike-1.6");
	EXPECT_EQ(m.Game().starter, L"cstrike.exe");
	EXPECT_EQ(m.Game().client,  L"cstrike.exe");
	EXPECT_EQ(m.Game().monitor_x86, L"GameMon.aes");
	EXPECT_EQ(m.Game().monitor_x64, L"GameMon64.aes");
}

TEST(ManifestLoad, ValidManifestParsesModules) {
	Manifest m;
	ASSERT_EQ(m.Load(L"fixtures/valid.manifest"), ManifestError::Ok);
	ASSERT_EQ(m.Modules().size(), 2u);
	EXPECT_EQ(m.Modules()[0].path, L"module_a.bin");
	EXPECT_EQ(m.Modules()[0].size, 128u);
	EXPECT_EQ(m.Modules()[0].sha256_hex.size(), 64u);
}

TEST(ManifestLoad, MalformedJsonReturnsError) {
	Manifest m;
	EXPECT_EQ(m.Load(L"fixtures/malformed.manifest"), ManifestError::MalformedJson);
}

TEST(ManifestLoad, MissingFileReturnsError) {
	Manifest m;
	EXPECT_EQ(m.Load(L"fixtures/does_not_exist.manifest"), ManifestError::FileMissing);
}
