/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 * loadfont.cpp
 *
 *  Read fonts stored in a .pbm file
 *
 */

#include "loadfont.h"

#include <cstdint>

#include <string>



Image read_pbm(FILE *img)
{
    enum class PBMState
    {
        Undefined,
        Comment,
        MagicP,
        Magic4,
        Whitespace,
        OptionalWhitespace,
        Width,
        Height,
        Raster
    } state = PBMState::MagicP,
      nextstate = PBMState::Undefined,
      oldstate = PBMState::Undefined;

    size_t idx = 0;
    size_t width = 0,
           height = 0;
    bool *data = nullptr;

    bool got_whitespace = false;
    std::string tmp = "";

    for (bool done = false; !done;)
    {
        uint8_t byte = 0;
        fread(&byte, sizeof(byte), 1, img);

        if (feof(img))
        {
            break;
        }

switch_on_byte:
        if (byte == '#' && state != PBMState::Raster)
        {
            oldstate = state;
            state = PBMState::Comment;
            //printf("Comment: '");
        }
        switch (state)
        {
        case PBMState::Undefined:
            printf("bad state: '%c'\n", byte);
            exit(EXIT_FAILURE);
            break;

        case PBMState::Comment:
            if (byte == '\n' || byte == '\r')
            {
                state = oldstate;
                oldstate = PBMState::Undefined;
                //printf("'\n");
            }
            else
            {
                //putchar(byte);
            }
            break;

        case PBMState::MagicP:
            if (byte == 'P')
            {
                //puts("MagicP");
                state = PBMState::Magic4;
            }
            else
            {
                printf("magicp: unexpected '%c'\n", byte);
                exit(EXIT_FAILURE);
            }
            break;

        case PBMState::Magic4:
            if (byte == '4')
            {
                //puts("Magic4");
                state = PBMState::Whitespace;
                nextstate = PBMState::Width;
            }
            else
            {
                printf("magic4: unexpected '%c'\n", byte);
                exit(EXIT_FAILURE);
            }
            break;

        case PBMState::Whitespace:
            if (isspace(byte))
            {
                //puts("Whitespace");
                got_whitespace = true;
            }
            else
            {
                if (got_whitespace)
                {
                    state = nextstate;
                    nextstate = PBMState::Undefined;
                    got_whitespace = false;
                    goto switch_on_byte;
                }
                else
                {
                    printf("whitespace: unexpected '%c'\n", byte);
                    exit(EXIT_FAILURE);
                }
            }
            break;

        case PBMState::OptionalWhitespace:
            if (!isspace(byte))
            {
                //puts("OptionalWhitespace");
                state = nextstate;
                nextstate = PBMState::Undefined;
                goto switch_on_byte;
            }
            break;


        case PBMState::Width:
        case PBMState::Height:
            if (isdigit(byte))
            {
                tmp += byte;
            }
            else
            {
                if (tmp.size() != 0)
                {
                    //puts("Width|Height");
                    if (state == PBMState::Width)
                    {
                        width = std::stoi(tmp);
                        //printf("width: %lu\n", width);
                        nextstate = PBMState::Height;
                    }
                    else
                    {
                        height = std::stoi(tmp);
                        //printf("height: %lu\n", height);
                        nextstate = PBMState::Raster;

                        data = new bool[width * height];
                    }
                    state = PBMState::Whitespace;
                    tmp = "";

                    goto switch_on_byte;
                }
                else
                {
                    printf("width/height: unexpected '%c'\n", byte);
                    exit(EXIT_FAILURE);
                }
            }
            break;

        case PBMState::Raster:
            if (idx >= width * height)
            {
                done = true;
            }
            else
            {
                for (size_t i = 0; i < 8; ++i)
                {
                    data[idx++] = ((byte >> (7-i)) & 1);
                    if (idx % width == 0)
                    {
                        break;
                    }
                }
            }
            break;
        }
    }

    rewind(img);

    Image out;
    out.data = std::unique_ptr<bool[]>(data);
    out.width = width;
    out.height = height;
    return out;
}


Font read_font(FILE *pbm)
{
    Image font_src = read_pbm(pbm);

    size_t const font_w = font_src.width / 8,
                 font_h = font_src.height / 16;

    Font glyphs;

    for (size_t f = 0; f < glyphs.size(); ++f)
    {
        size_t img_x = (f % 8) * font_w,
               img_y = (f / 8) * font_h;

        glyphs[f].data =\
            std::unique_ptr<bool[]>(
                new bool[font_h * font_h]);
        glyphs[f].width = font_w;
        glyphs[f].height = font_h;

        for (size_t y = 0; y < font_h; ++y)
        {
            for (size_t x = 0; x < font_w; ++x)
            {
                glyphs[f].set(
                    x, y,
                    font_src.get(
                        img_x + x,
                        img_y + y));
            }
        }
    }

    return glyphs;
}

void write_pbm(FILE *out, Image const in)
{
    fprintf(out, "P4\n");
    fprintf(out, "%lu %lu\n", in.width, in.height);

    for (size_t y = 0; y < in.height; ++y)
    {
        for (size_t x = 0; x < in.width / 8; ++x)
        {
            uint8_t byte = 0;
            for (size_t b = 0; b < 8; ++b)
            {
                byte |= in.get(x*8 + b, y) << (7-b);
            }
            fprintf(out, "%c", byte);
        }
    }
}

