#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <sodium.h>

static int Usage()
{
	std::fprintf(stderr,
		"usage: sign-manifest --gen-keypair <label>\n"
		"       sign-manifest --sign <manifest-path> --key <private-key-path>\n");
	return 2;
}

static std::string HexEncode(const uint8_t* b, size_t n)
{
	static const char* d = "0123456789abcdef";
	std::string s;
	s.reserve(n * 2);
	for (size_t i = 0; i < n; ++i)
	{
		s.push_back(d[b[i] >> 4]);
		s.push_back(d[b[i] & 0xF]);
	}
	return s;
}

static int GenKeypair(const std::string& label)
{
	uint8_t pk[crypto_sign_ed25519_PUBLICKEYBYTES];
	uint8_t sk[crypto_sign_ed25519_SECRETKEYBYTES];
	crypto_sign_ed25519_keypair(pk, sk);

	std::ofstream sfout("hac-" + label + ".private.key", std::ios::binary);
	if (!sfout)
	{
		std::fprintf(stderr, "cannot write private key\n");
		return 1;
	}
	sfout.write(reinterpret_cast<const char*>(sk), sizeof(sk));
	sfout.close();

	std::ofstream pfout("hac-" + label + ".public.key.hex");
	if (!pfout)
	{
		std::fprintf(stderr, "cannot write public key\n");
		return 1;
	}
	pfout << HexEncode(pk, sizeof(pk));
	return 0;
}

static int SignFile(const std::string& manifest, const std::string& privkey)
{
	std::ifstream mf(manifest, std::ios::binary);
	if (!mf)
	{
		std::fprintf(stderr, "cannot open manifest\n");
		return 1;
	}
	std::vector<uint8_t> payload((std::istreambuf_iterator<char>(mf)), {});

	std::ifstream kf(privkey, std::ios::binary);
	if (!kf)
	{
		std::fprintf(stderr, "cannot open private key\n");
		return 1;
	}
	std::vector<uint8_t> sk((std::istreambuf_iterator<char>(kf)), {});
	if (sk.size() != crypto_sign_ed25519_SECRETKEYBYTES)
	{
		std::fprintf(stderr, "private key wrong size (%zu, expected %d)\n",
			sk.size(), crypto_sign_ed25519_SECRETKEYBYTES);
		return 1;
	}

	uint8_t sig[crypto_sign_ed25519_BYTES];
	unsigned long long siglen = 0;
	if (crypto_sign_ed25519_detached(sig, &siglen, payload.data(), payload.size(), sk.data()) != 0)
	{
		std::fprintf(stderr, "signing failed\n");
		return 1;
	}

	std::ofstream out(manifest + ".sig", std::ios::binary);
	if (!out)
	{
		std::fprintf(stderr, "cannot write .sig\n");
		return 1;
	}
	out.write(reinterpret_cast<const char*>(sig), siglen);
	return 0;
}

int main(int argc, char** argv)
{
	if (sodium_init() < 0)
	{
		std::fprintf(stderr, "sodium_init failed\n");
		return 1;
	}
	if (argc == 3 && std::strcmp(argv[1], "--gen-keypair") == 0)
	{
		return GenKeypair(argv[2]);
	}
	if (argc == 5 && std::strcmp(argv[1], "--sign") == 0 && std::strcmp(argv[3], "--key") == 0)
	{
		return SignFile(argv[2], argv[4]);
	}
	return Usage();
}
