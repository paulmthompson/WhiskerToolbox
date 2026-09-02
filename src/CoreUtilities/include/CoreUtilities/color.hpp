#ifndef DATAMANAGER_COLOR_HPP
#define DATAMANAGER_COLOR_HPP

#include "coreutilities_export.h"

#include <string>

bool isValidHexColor(const std::string& hex_color);

bool isValidAlpha(float alpha);

COREUTILITIES_EXPORT std::string generateRandomColor();

COREUTILITIES_EXPORT void hexToRGB(const std::string &hexColor, int &r, int &g, int &b);

COREUTILITIES_EXPORT void hexToRGB(const std::string & hexColor, float & r, float & g, float & b);


#endif // COLOR_HPP
