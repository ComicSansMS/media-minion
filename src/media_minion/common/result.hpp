#ifndef MEDIA_MINION_INCLUDE_GUARD_COMMON_RESULT_HPP_
#define MEDIA_MINION_INCLUDE_GUARD_COMMON_RESULT_HPP_

#include <gbBase/Assert.hpp>

#include <boost/outcome/result.hpp>
#include <boost/outcome/try.hpp>

#include <system_error>

#define MM_OUTCOME_PROPAGATE_IMPL(expr, unique_name) \
{ \
    BOOST_OUTCOME_TRY(unique_name, expr); \
    return unique_name; \
} (void)(0)

#define MM_OUTCOME_PROPAGATE(expr) MM_OUTCOME_PROPAGATE_IMPL(expr, BOOST_OUTCOME_TRY_UNIQUE_NAME)

namespace media_minion {

enum class errc {
    decode_error = 1,
    network_error,
};

const std::error_category& media_minion_error();

inline std::error_code make_error_code(errc e) {
    return std::error_code{static_cast<int>(e), media_minion_error() };
}

}

namespace std {

template<>
struct is_error_code_enum<media_minion::errc> : ::std::true_type {};

}

namespace media_minion {

template<typename T>
using Result = boost::outcome_v2::result<T, std::error_code, boost::outcome_v2::policy::terminate>;

}

#endif
