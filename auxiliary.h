#ifndef AUXILIARY_H
#define AUXILIARY_H

#include <boost/property_tree/xml_parser.hpp>

#include <sstream>
#include <string>

class MakeString
{
public:
    template <typename T>
    MakeString& operator<<(const T& arg)
    {
        m_stream << arg;
        return *this;
    }

    operator std::string() const
    {
        return m_stream.str();
    }

protected:
    std::ostringstream m_stream;
};

inline
std::string get_string_between(const std::string& input, const std::string& first, const std::string& second)
{
    const auto first_string_begin = input.find(first);
    const auto first_string_end = first_string_begin + first.size();
    const auto second_string_begin = input.find(second, first_string_end);
    return input.substr(first_string_end, second_string_begin - first_string_end);
}

#endif // AUXILIARY_H
