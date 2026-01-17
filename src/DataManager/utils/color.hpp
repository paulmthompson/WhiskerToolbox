#ifndef DATAMANAGER_COLOR_HPP
#define DATAMANAGER_COLOR_HPP

#include "datamanager_export.h"

#include <string>

bool DATAMANAGER_EXPORT isValidHexColor(const std::string& hex_color);

bool isValidAlpha(float alpha);

std::string DATAMANAGER_EXPORT generateRandomColor();

void DATAMANAGER_EXPORT hexToRGB(const std::string &hexColor, int &r, int &g, int &b);

void DATAMANAGER_EXPORT hexToRGB(const std::string & hexColor, float & r, float & g, float & b);


#endif // COLOR_HPP
