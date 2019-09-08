#include <media_minion/common/result.hpp>

namespace media_minion {

namespace {
struct ErrorCategory : std::error_category {
public:
    ErrorCategory() = default;
    ~ErrorCategory() override = default;

    ErrorCategory(ErrorCategory const&) = delete;
    ErrorCategory(ErrorCategory&&) = delete;

    char const* name() const noexcept override {
        return "media_minion";
    }

    std::string message(int ev) const noexcept override {
        switch (static_cast<errc>(ev)) {
        case errc::decode_error:    return "Decode Error";
        case errc::network_error:   return "Network Error";
        default:                    return "Unknown Error";
        }
    }
};

ErrorCategory g_error_category{};
}

const std::error_category& media_minion_error()
{
    return g_error_category;
}

}
