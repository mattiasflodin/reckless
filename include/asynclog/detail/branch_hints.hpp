#ifndef ASYNCLOG_DETAIL_BRANCH_HINTS_HPP
#define ASYNCLOG_DETAIL_BRANCH_HINTS_HPP
namespace asynclog {
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

#endif  // ASYNCLOG_DETAIL_BRANCH_HINTS_HPP
