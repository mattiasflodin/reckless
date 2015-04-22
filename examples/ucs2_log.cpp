#include <reckless/basic_log.hpp>
#include <reckless/file_writer.hpp>
#include <string>

class ucs2_log : public reckless::basic_log {
public:
    ucs2_log(reckless::writer* pwriter) : basic_log(pwriter) {}
    void puts(std::wstring const& s)
    {
        basic_log::write<ucs2_formatter>(s);
    }
    void puts(std::wstring&& s)
    {
        basic_log::write<ucs2_formatter>(std::move(s));
    }
private:
    struct ucs2_formatter {
        static void format(reckless::output_buffer* pbuffer, std::wstring const& s)
        {
            pbuffer->write(s.data(), sizeof(wchar_t)*s.size());
        }
    };
};

reckless::file_writer writer("log.txt");
ucs2_log g_log(&writer);

int main()
{
    std::wstring s(L"Hello World!\n");
    for(std::size_t i=0; i!=10000; ++i)
        g_log.puts(s);
        //g_log.puts(L"Hello World!\n");
    g_log.close();
    return 0;
}

