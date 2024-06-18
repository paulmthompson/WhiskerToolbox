#ifndef STRING_MANIP_HPP
#define STRING_MANIP_HPP

#include <iostream>
#include <string>
#include <regex>

//https://stackoverflow.com/questions/30073839/c-extract-number-from-the-middle-of-a-string
inline std::string extract_numbers_from_string(std::string const & input)
{
    std::string output = std::regex_replace(
        input,
        std::regex("[^0-9]*([0-9]+).*"),
        std::string("$1")
        );
    std::cout << input << std::endl;
    std::cout << output << std::endl;

    return output;
};


#endif // STRING_MANIP_HPP
