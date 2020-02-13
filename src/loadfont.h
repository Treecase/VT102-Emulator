/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 * loadfont.h
 *
 *  Reading font files.
 *  Fonts are expected to be contained in a .pbm file.
 *  This file contains the 128 glyphs in a grid,
 *  where the grid is 8 glyphs wide and 16 glyphs tall.
 *
 */

#ifndef _LOADFONT_H
#define _LOADFONT_H


#include <cstdio>
#include <cstring>

#include <memory>
#include <array>


struct Image
{
    std::unique_ptr<bool[]> data;
    size_t width,
           height;

    bool get(size_t x, size_t y) const
    {
        if (x < width && y < height)
        {
            return data[(y * width) + x];
        }
        else
        {
            throw std::out_of_range(
                x < width? "y" : "x"
                " out of bounds");
        }
    }

    void set(size_t x, size_t y, bool pixel)
    {
        if (x < width && y < height)
        {
            data[(y * width) + x] = pixel;
        }
        else
        {
            throw std::out_of_range(
                x < width? "y" : "x"
                " out of bounds");
        }
    }


    Image()
    :   data(nullptr),
        width(0),
        height(0)
    {
    }

    Image(Image const &other)
    {
        if (&other != this)
        {
            width = other.width;
            height = other.height;
            data = std::unique_ptr<bool[]>(
                new bool[width * height]);
            memcpy(data.get(), other.data.get(), width * height);
        }
    }
};


typedef std::array<Image, 128> Font;


Font read_font(FILE *img);
Image read_pbm(FILE *img);
void write_pbm(FILE *out, Image const img);


#endif

