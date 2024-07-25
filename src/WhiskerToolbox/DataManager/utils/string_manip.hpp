#ifndef STRING_MANIP_HPP
#define STRING_MANIP_HPP

#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>


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

//https://stackoverflow.com/questions/6417817/easy-way-to-remove-extension-from-a-filename
inline std::string remove_extension(std::string const & filename) {
    const size_t lastdot = filename.find_last_of(".");
    if (lastdot == std::string::npos) return filename;
    return filename.substr(0, lastdot);
}

/**
 *
 * Creates a string of a number that is left padded with zeros
 * to be pad_digits long.
 *
 * @brief pad_frame_id
 * @param number_to_pad
 * @param pad_digits
 * @return
 */
inline std::string pad_frame_id(int const number_to_pad, int const pad_digits)
{
    std::stringstream ss;
    ss << std::setw(pad_digits) << std::setfill('0') << number_to_pad;

    return ss.str();
}

// https://www.cespedes.org/blog/85/how-to-escape-latex-special-characters
// For now only underscores are needed
inline std::string escape_latex(std::string s){
    return std::regex_replace(s, std::regex("_"), std::string("\\_"));
}

#endif // STRING_MANIP_HPP
