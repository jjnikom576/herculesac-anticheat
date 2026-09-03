#include "Manifest.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <json/json.h>
#include <sodium.h>
#include <Windows.h>

namespace hac::manifest {

namespace {

constexpr size_t kMaxManifestBytes = 1 * 1024 * 1024; // 1 MiB

std::wstring Widen(const std::string& s) {
	if (s.empty()) return {};
	int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
	std::wstring out(static_cast<size_t>(n - 1), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), n);
	return out;
}

ManifestError ReadFileBytes(const std::wstring& path, std::vector<uint8_t>& out) {
	std::ifstream f(path, std::ios::binary);
	if (!f) return ManifestError::FileMissing;
	f.seekg(0, std::ios::end);
	auto len = f.tellg();
	if (len < 0) return ManifestError::FileMissing;
	if (static_cast<size_t>(len) > kMaxManifestBytes) return ManifestError::TooLarge;
	f.seekg(0, std::ios::beg);
	out.resize(static_cast<size_t>(len));
	if (!out.empty()) f.read(reinterpret_cast<char*>(out.data()), out.size());
	return ManifestError::Ok;
}

std::string BytesToHex(const uint8_t* b, size_t n)
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

std::wstring Basename(const std::wstring& p)
{
	auto pos = p.find_last_of(L"/\\");
	return pos == std::wstring::npos ? p : p.substr(pos + 1);
}

} // namespace

ManifestError Manifest::Load(const std::wstring& manifest_path) {
	if (auto e = ReadFileBytes(manifest_path, raw_bytes_); e != ManifestError::Ok) return e;

	// Sibling .sig file — missing is not fatal here; VerifySignature will fail later.
	std::vector<uint8_t> sig;
	(void)ReadFileBytes(manifest_path + L".sig", sig);
	signature_ = std::move(sig);

	install_root_ = std::filesystem::path(manifest_path).parent_path().wstring();
	if (!install_root_.empty() && install_root_.back() != L'\\' && install_root_.back() != L'/') {
		install_root_ += L'\\';
	}

	Json::CharReaderBuilder builder;
	std::string errs;
	Json::Value root;
	std::istringstream in(std::string(raw_bytes_.begin(), raw_bytes_.end()));
	if (!Json::parseFromStream(builder, in, &root, &errs)) return ManifestError::MalformedJson;

	if (!root.isMember("version") || root["version"].asInt() != 2) return ManifestError::UnsupportedVersion;

	const auto& g = root["game"];
	game_.id           = g["id"].asString();
	game_.starter      = Widen(g["starter"].asString());
	game_.client       = Widen(g["client"].asString());
	game_.monitor_x86  = Widen(g["monitor_x86"].asString());
	game_.monitor_x64  = Widen(g["monitor_x64"].asString());

	modules_.clear();
	for (const auto& m : root["modules"]) {
		ModuleEntry me;
		me.path       = Widen(m["path"].asString());
		me.sha256_hex = m["sha256"].asString();
		me.size       = m["size"].asUInt64();
		modules_.push_back(std::move(me));
	}

	whitelist_.clear();
	for (const auto& w : root["whitelist_sha256"]) {
		whitelist_.push_back(w.asString());
	}

	const auto& r = root["reporting"];
	reporting_.endpoint                  = Widen(r["endpoint"].asString());
	reporting_.server_cert_pin_sha256_hex = r["server_cert_pin_sha256"].asString();

	return ManifestError::Ok;
}

ManifestError Manifest::VerifySignature(const uint8_t (&public_key)[32]) const
{
	if (signature_.size() != crypto_sign_ed25519_BYTES) return ManifestError::SignatureInvalid;
	if (raw_bytes_.empty())                              return ManifestError::SignatureInvalid;
	if (sodium_init() < 0)                               return ManifestError::SignatureInvalid;
	if (crypto_sign_ed25519_verify_detached(
			signature_.data(),
			raw_bytes_.data(), raw_bytes_.size(),
			public_key) != 0)
	{
		return ManifestError::SignatureInvalid;
	}
	return ManifestError::Ok;
}

ManifestError Manifest::VerifyModule(const std::wstring& module_path) const
{
	std::wstring base = Basename(module_path);
	auto it = std::find_if(modules_.begin(), modules_.end(),
		[&](const ModuleEntry& m) { return Basename(m.path) == base; });
	if (it == modules_.end())
	{
		return ManifestError::ModuleMissing;
	}

	std::ifstream f(module_path, std::ios::binary);
	if (!f)
	{
		return ManifestError::ModuleMissing;
	}
	f.seekg(0, std::ios::end);
	auto len = f.tellg();
	if (len < 0)
	{
		return ManifestError::ModuleMissing;
	}
	if (static_cast<uint64_t>(len) != it->size)
	{
		return ManifestError::ModuleSizeMismatch;
	}
	f.seekg(0, std::ios::beg);

	if (sodium_init() < 0)
	{
		return ManifestError::ModuleMissing;
	}
	crypto_hash_sha256_state st;
	crypto_hash_sha256_init(&st);
	constexpr size_t kBuf = 64 * 1024;
	std::vector<uint8_t> buf(kBuf);
	while (f)
	{
		f.read(reinterpret_cast<char*>(buf.data()), buf.size());
		auto got = static_cast<size_t>(f.gcount());
		if (got == 0)
		{
			break;
		}
		crypto_hash_sha256_update(&st, buf.data(), got);
	}
	uint8_t out[crypto_hash_sha256_BYTES];
	crypto_hash_sha256_final(&st, out);

	if (BytesToHex(out, sizeof(out)) != it->sha256_hex)
	{
		return ManifestError::ModuleHashMismatch;
	}
	return ManifestError::Ok;
}

} // namespace hac::manifest
