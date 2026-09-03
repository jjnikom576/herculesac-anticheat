#include "Manifest.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <json/json.h>
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

// VerifySignature and VerifyModule remain stubs — Tasks 5 and 6 fill them in.
ManifestError Manifest::VerifySignature(const uint8_t (&)[32]) const { return ManifestError::Ok; }
ManifestError Manifest::VerifyModule(const std::wstring&) const     { return ManifestError::Ok; }

} // namespace hac::manifest
