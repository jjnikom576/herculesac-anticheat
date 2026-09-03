#include "Manifest.h"

namespace hac::manifest {

ManifestError Manifest::Load(const std::wstring& /*manifest_path*/) {
	return ManifestError::Ok;
}

ManifestError Manifest::VerifySignature(const uint8_t (&/*public_key*/)[32]) const {
	return ManifestError::Ok;
}

ManifestError Manifest::VerifyModule(const std::wstring& /*module_path*/) const {
	return ManifestError::Ok;
}

} // namespace hac::manifest
