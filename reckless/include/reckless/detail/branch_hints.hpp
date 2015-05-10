#ifndef RECKLESS_DETAIL_BRANCH_HINTS_HPP
#define RECKLESS_DETAIL_BRANCH_HINTS_HPP
namespace reckless {
namespace detail {

inline bool likely(bool expr) {
#ifdef __GNUC__
    return __builtin_expect(expr, true);
#else
    return expr;
#endif
}

inline bool unlikely(bool expr)
{
#ifdef __GNUC__
    return __builtin_expect(expr, false);
#else
    return expr;
#endif
}

}
}

#endif  // RECKLESS_DETAIL_BRANCH_HINTS_HPP
