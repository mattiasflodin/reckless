#include <boost/date_time/posix_time/posix_time_types.hpp>

#include <boost/log/trivial.hpp>
#include <boost/move/utility_core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/support/date_time.hpp>

namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace src = boost::log::sources;
namespace expr = boost::log::expressions;
namespace attrs = boost::log::attributes;
namespace keywords = boost::log::keywords;

enum severity_level
{
    debug,
    information,
    warning,
    error
};
struct severity_tag;

#ifdef LOG_ONLY_DECLARE
extern src::severity_logger_mt<severity_level> g_logger;
#else
src::severity_logger_mt<severity_level> g_logger;
#endif

inline logging::formatting_ostream& operator<<
(
    logging::formatting_ostream& strm,
    logging::to_log_manip< severity_level, severity_tag > const& manip
)
{
    char levels[] = { 'D', 'I', 'W', 'E' };

    severity_level level = manip.get();
    strm << levels[level];

    return strm;
}

#define LOG_INIT(queue_size) \
    logging::add_file_log( \
        keywords::file_name = "log.txt", \
        keywords::format = ( \
            expr::stream \
            << expr::attr<severity_level, severity_tag>("Severity") \
            << ' ' \
            << expr::format_date_time<boost::posix_time::ptime>( \
                "TimeStamp", "%Y-%m-%d %H:%M:%S.%f") \
            << ' ' << expr::smessage \
        ) \
    ); \
    logging::add_common_attributes();

#define LOG_CLEANUP()

#define LOG( c, i, f ) \
    logging::record rec = g_logger.open_record(keywords::severity = severity_level::information); \
    if (rec) \
    { \
        logging::record_ostream strm(rec); \
        strm << "Hello, World! " << c << ' ' << i << ' ' << f; \
        strm.flush(); \
        g_logger.push_record(boost::move(rec)); \
    }

#define LOG_FILE_WRITE(FileNumber, Percent) \
    logging::record rec = g_logger.open_record(keywords::severity = severity_level::information); \
    if (rec) \
    { \
        logging::record_ostream strm(rec); \
        strm << "file " << FileNumber << " (" << Percent << "%)"; \
        strm.flush(); \
        g_logger.push_record(boost::move(rec)); \
    }

#define LOG_MANDELBROT(Thread, X, Y, FloatX, FloatY, Iterations) \
    logging::record rec = g_logger.open_record(keywords::severity = severity_level::information); \
    if (rec) \
    { \
        logging::record_ostream strm(rec); \
        strm << "[T" << Thread << "] " << X << ',' << Y << '/' << FloatX << ',' << FloatY << ": " << Iterations << " iterations"; \
        strm.flush(); \
        g_logger.push_record(boost::move(rec)); \
    }
