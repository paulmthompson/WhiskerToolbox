#ifndef COLOR_SCHEME_HPP
#define COLOR_SCHEME_HPP

#include <QPalette>

// This color scheme is from ezEngine
//https://github.com/ezEngine/ezEngine
/*
 *
 * MIT License

Copyright 2024 ezEngine Project

Permission is hereby granted, free of charge, to any person obtaining a copy of this
software and associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
 */



QPalette create_palette()
{

    QPalette palette;

    //ezColorGammaUB highlightColor = ezColorScheme::DarkUI(ezColorScheme::Yellow);
    //ezColorGammaUB highlightColorDisabled = ezColorScheme::DarkUI(ezColorScheme::Yellow) * 0.5f;
    //ezColorGammaUB linkVisitedColor = ezColorScheme::LightUI(ezColorScheme::Yellow);

    QBrush const NoRoleBrush(QColor(0, 0, 0), Qt::NoBrush);
    palette.setBrush(QPalette::NoRole, NoRoleBrush);

    palette.setColor(QPalette::WindowText, QColor(255, 255, 255));          // labels, tabs, property grid
    palette.setColor(QPalette::Button, QColor(51, 51, 51));                 // buttons, toolbuttons, dashboard background
    palette.setColor(QPalette::Light, QColor(81, 58, 47));                  // lines between tabs, inactive tab gradient
    palette.setColor(QPalette::Dark, QColor(0, 0, 0));                      // line below active window highlight
    palette.setColor(QPalette::Mid, QColor(90, 90, 90));                    // color of the box around component properties (collapsible group box)
    palette.setColor(QPalette::Text, QColor(204, 204, 204));                // scene graph, values in spin boxes, checkmarks
    palette.setColor(QPalette::ButtonText, QColor(204, 204, 204));          // menus, comboboxes, headers
    palette.setColor(QPalette::Base, QColor(24, 24, 24));                   // background inside complex windows (scenegraph)
    palette.setColor(QPalette::Window, QColor(42, 42, 42));                 // window borders, toolbars
    palette.setColor(QPalette::Shadow, QColor(70, 70, 70));                 // background color for arrays in property grids
    //palette.setColor(QPalette::Highlight, ezToQtColor(highlightColor));     // selected items
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));     // text of selected items
    palette.setColor(QPalette::Link, QColor(104, 205, 254));                // manipulator links in property grid
    //palette.setColor(QPalette::LinkVisited, ezToQtColor(linkVisitedColor)); // manipulator links in property grid when active
    palette.setColor(QPalette::AlternateBase, QColor(49, 49, 49));          // second base color, mainly used for alternate row colors
    palette.setColor(QPalette::PlaceholderText, QColor(142, 142, 142));     // text in search fields

    palette.setColor(QPalette::Midlight, QColor(58, 58, 58));               // unused ?
    palette.setColor(QPalette::BrightText, QColor(221, 221, 221));          // unused ?
    palette.setColor(QPalette::ToolTipBase, QColor(52, 52, 52));            // unused / not working ?
    palette.setColor(QPalette::ToolTipText, QColor(221, 221, 221));         // unused / not working ?

    palette.setColor(QPalette::Disabled, QPalette::Window, QColor(25, 25, 25));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(128, 128, 128));
    palette.setColor(QPalette::Disabled, QPalette::Button, QColor(35, 35, 35));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(105, 105, 105));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(128, 128, 128));

    return palette;
}


#endif // COLOR_SCHEME_HPP
