#pragma once

#include <cstdint>

namespace dare {
	class UUID {
	public:
		UUID();
		UUID(uint64_t _uuid);

		operator uint64_t() const { return uuid; }
	private:
		uint64_t uuid;
	};
}

namespace std {
	template <typename T> struct hash;

	template<>
	struct hash<dare::UUID> {
		std::size_t operator()(const dare::UUID& uuid) const {
			return static_cast<uint64_t>(uuid);
		}
	};
}