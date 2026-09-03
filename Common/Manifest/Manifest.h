#pragma once

#ifndef _HAC_MANIFEST_H
#define _HAC_MANIFEST_H

#include <cstdint>
#include <string>
#include <vector>
#include "Errors.h"

namespace hac::manifest {

struct ModuleEntry {
	std::wstring path;
	std::string  sha256_hex;     // 64 lowercase hex chars
	uint64_t     size = 0;
};

struct GameConfig {
	std::string  id;
	std::wstring starter;
	std::wstring client;
	std::wstring monitor_x86;
	std::wstring monitor_x64;
};

struct ReportingConfig {
	std::wstring endpoint;
	std::string  server_cert_pin_sha256_hex;
};

class Manifest {
public:
	Manifest() = default;

	// Reads manifest_path and manifest_path + ".sig" from disk.
	// Does not verify signatures — call VerifySignature() next.
	ManifestError Load(const std::wstring& manifest_path);

	// Verifies the Ed25519 detached signature over the raw manifest bytes.
	ManifestError VerifySignature(const uint8_t (&public_key)[32]) const;

	// Locates module_path in modules[], hashes the file on disk, compares.
	ManifestError VerifyModule(const std::wstring& module_path) const;

	// Returns the base directory the manifest lives in — used by callers to
	// resolve module paths relative to install root.
	const std::wstring& InstallRoot() const noexcept { return install_root_; }

	const GameConfig&                Game()             const noexcept { return game_; }
	const std::vector<ModuleEntry>&  Modules()          const noexcept { return modules_; }
	const std::vector<std::string>&  WhitelistSha256()  const noexcept { return whitelist_; }
	const ReportingConfig&           Reporting()        const noexcept { return reporting_; }

private:
	std::wstring              install_root_;
	std::vector<uint8_t>      raw_bytes_;
	std::vector<uint8_t>      signature_;
	GameConfig                game_;
	std::vector<ModuleEntry>  modules_;
	std::vector<std::string>  whitelist_;
	ReportingConfig           reporting_;
};

} // namespace hac::manifest

#endif
