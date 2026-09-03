#pragma once

#ifndef _HAC_MANIFEST_ERRORS_H
#define _HAC_MANIFEST_ERRORS_H

#include <string_view>

namespace hac::manifest {

enum class ManifestError {
	Ok = 0,
	FileMissing,
	TooLarge,             // > 1 MiB
	MalformedJson,
	UnsupportedVersion,
	SignatureInvalid,
	ModuleMissing,
	ModuleSizeMismatch,
	ModuleHashMismatch,
};

constexpr std::string_view ToString(ManifestError e) noexcept {
	switch (e) {
		case ManifestError::Ok:                 return "Ok";
		case ManifestError::FileMissing:        return "FileMissing";
		case ManifestError::TooLarge:           return "TooLarge";
		case ManifestError::MalformedJson:      return "MalformedJson";
		case ManifestError::UnsupportedVersion: return "UnsupportedVersion";
		case ManifestError::SignatureInvalid:   return "SignatureInvalid";
		case ManifestError::ModuleMissing:      return "ModuleMissing";
		case ManifestError::ModuleSizeMismatch: return "ModuleSizeMismatch";
		case ManifestError::ModuleHashMismatch: return "ModuleHashMismatch";
	}
	return "Unknown";
}

} // namespace hac::manifest

#endif
