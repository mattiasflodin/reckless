#include <reckless/writer.hpp>

namespace reckless {
    
namespace {
class error_category_t : public std::error_category {
public:
    char const* name() const noexcept override;
    std::error_condition default_error_condition(int code) const noexcept override;
    std::string message(int condition) const override;
};

}   // anonymous namespace

writer::~writer()
{
}

std::error_category const& writer::error_category()
{
    static error_category_t ec;
    return ec;
}

char const* error_category_t::name() const noexcept
{
    return "reckless::writer";
}

std::error_condition error_category_t::default_error_condition(int code) const noexcept
{
    return static_cast<writer::errc>(code);
}

std::string error_category_t::message(int condition) const
{
    switch(static_cast<writer::errc>(condition)) {
    case writer::temporary_failure:
        return "temporary failure while writing log";
    case writer::permanent_failure:
        return "permanent failure while writing log";
    }
    throw std::invalid_argument("invalid condition code");
}

}   // namespace reckless
