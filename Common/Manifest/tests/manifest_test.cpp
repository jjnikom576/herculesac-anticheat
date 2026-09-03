#include <gtest/gtest.h>
#include "../Manifest.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using namespace hac::manifest;

namespace {

	std::vector<uint8_t> HexToBytes(const std::string& hex)
	{
		std::vector<uint8_t> out(hex.size() / 2);
		for (size_t i = 0; i < out.size(); ++i)
		{
			unsigned v;
			std::sscanf(hex.c_str() + 2 * i, "%2x", &v);
			out[i] = static_cast<uint8_t>(v);
		}
		return out;
	}

	void LoadPubKey(uint8_t (&out)[32])
	{
		std::ifstream f("fixtures/test_pubkey.hex");
		std::string hex;
		std::getline(f, hex);
		auto bytes = HexToBytes(hex);
		std::memcpy(out, bytes.data(), 32);
	}

} // namespace

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

TEST(ManifestVerify, ValidSignatureAccepted) {
	Manifest m;
	ASSERT_EQ(m.Load(L"fixtures/valid.manifest"), ManifestError::Ok);
	uint8_t pk[32];
	LoadPubKey(pk);
	EXPECT_EQ(m.VerifySignature(pk), ManifestError::Ok);
}

TEST(ManifestVerify, WrongSignatureRejected) {
	// fixtures/valid_wrongsig.manifest is byte-identical to valid.manifest,
	// but its sibling .sig is signed by an unrelated key.
	Manifest m;
	ASSERT_EQ(m.Load(L"fixtures/valid_wrongsig.manifest"), ManifestError::Ok);
	uint8_t pk[32];
	LoadPubKey(pk);
	EXPECT_EQ(m.VerifySignature(pk), ManifestError::SignatureInvalid);
}

TEST(ManifestVerify, TamperedPayloadRejected) {
	Manifest m;
	ASSERT_EQ(m.Load(L"fixtures/tampered.manifest"), ManifestError::Ok);
	uint8_t pk[32];
	LoadPubKey(pk);
	EXPECT_EQ(m.VerifySignature(pk), ManifestError::SignatureInvalid);
}
