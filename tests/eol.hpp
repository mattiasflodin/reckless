#include <string>

std::string eol(std::string const& s)
{
#if defined(__unix__)
    return s;
#elif defined(_WIN32)
    std::string res;
    res.reserve(s.size());
    for(char c : s) {
        if('\n' == c)
            res.push_back('\r');
        res.push_back(c);
    }
    return res;
#endif
}
