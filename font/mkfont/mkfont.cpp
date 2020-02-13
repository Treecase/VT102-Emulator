/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 * mkfont.cpp
 *
 *  Build the font images from the given font source
 *
 */

#include "../../src/loadfont.h"



int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("need 1 arg\n");
        exit(1);
    }

    FILE *src_file = fopen(argv[1], "r");
    Image src = read_pbm(src_file);
    fclose(src_file);

    size_t const font_w = src.width  /  8,
                 font_h = src.height / 16;


    /* ========== 80 COLUMN FONTS ========== */
    {
    /* build the normal font */
    Image modified{};
    modified.width = src.width + (2*font_w);
    modified.height = src.height * 2;
    modified.data = std::unique_ptr<bool[]>(
        new bool[modified.width * modified.height]);
    memset(modified.data.get(), 1, modified.width * modified.height);

    for (size_t y = 0; y < src.height; ++y)
    {
        for (size_t x = 0; x < 8; ++x)
        {
            for (size_t gx = 0; gx < font_w; ++gx)
            {
                if (src.get(x * font_w + gx, y) == 0)
                {
                    modified.set(x*(font_w+2) + gx  , y * 2, 0);
                    modified.set(x*(font_w+2) + gx+1, y * 2, 0);
                }
            }
            if (src.get(x * font_w + font_w-1, y) == 0)
            {
                modified.set(x*(font_w+2) + font_w+1, y * 2, 0);
            }
        }
    }

    FILE *out = fopen("80col-normal.pbm", "w");
    write_pbm(out, modified);
    fclose(out);


    /* build the double-width font */
    Image dw{};
    dw.width = modified.width * 2;
    dw.height = modified.height;
    dw.data = std::unique_ptr<bool[]>(
        new bool[dw.width * dw.height]);
    memset(dw.data.get(), 1, dw.width * dw.height);

    for (size_t y = 0; y < src.height; ++y)
    {
        for (size_t x = 0; x < 8; ++x)
        {
            for (size_t gx = 0; gx < font_w; ++gx)
            {
                if (src.get(x * font_w + gx, y) == 0)
                {
                    dw.set(2*x*(font_w+2) + (2*gx)  , y * 2, 0);
                    dw.set(2*x*(font_w+2) + (2*gx)+1, y * 2, 0);
                    dw.set(2*x*(font_w+2) + (2*gx)+2, y * 2, 0);
                }
            }
            if (src.get(x * font_w + font_w-1, y) == 0)
            {
                dw.set(2*x*(font_w+2) + (2*font_w)+1, y * 2, 0);
                dw.set(2*x*(font_w+2) + (2*font_w)+2, y * 2, 0);
                dw.set(2*x*(font_w+2) + (2*font_w)+3, y * 2, 0);
            }
        }
    }

    out = fopen("80col-doublewidth.pbm", "w");
    write_pbm(out, dw);
    fclose(out);


    /* build the double-height font */
    Image dh{};
    dh.width = dw.width;
    dh.height = dw.height * 2;
    dh.data = std::unique_ptr<bool[]>(
        new bool[dh.width * dh.height]);
    memset(dh.data.get(), 1, dh.width * dh.height);

    for (size_t y = 0; y < src.height; ++y)
    {
        for (size_t x = 0; x < 8; ++x)
        {
            for (size_t gx = 0; gx < font_w; ++gx)
            {
                if (src.get(x * font_w + gx, y) == 0)
                {
                    dh.set(2*x*(font_w+2) + (2*gx)  , y * 4, 0);
                    dh.set(2*x*(font_w+2) + (2*gx)+1, y * 4, 0);
                    dh.set(2*x*(font_w+2) + (2*gx)+2, y * 4, 0);

                    dh.set(2*x*(font_w+2) + (2*gx)  , y * 4 + 2, 0);
                    dh.set(2*x*(font_w+2) + (2*gx)+1, y * 4 + 2, 0);
                    dh.set(2*x*(font_w+2) + (2*gx)+2, y * 4 + 2, 0);
                }
            }
            if (src.get(x * font_w + font_w-1, y) == 0)
            {
                dh.set(2*x*(font_w+2) + (2*font_w)+1, y * 4, 0);
                dh.set(2*x*(font_w+2) + (2*font_w)+2, y * 4, 0);
                dh.set(2*x*(font_w+2) + (2*font_w)+3, y * 4, 0);

                dh.set(2*x*(font_w+2) + (2*font_w)+1, y * 4+2, 0);
                dh.set(2*x*(font_w+2) + (2*font_w)+2, y * 4+2, 0);
                dh.set(2*x*(font_w+2) + (2*font_w)+3, y * 4+2, 0);
            }
        }
    }

    out = fopen("80col-doubleheight.pbm", "w");
    write_pbm(out, dh);
    fclose(out);
    }
    /* ========== END OF 80 COLUMN FONTS ========== */



    /* ========== 132 COLUMN FONTS ========== */
    {
    /* build the normal font */
    Image modified{};
    modified.width = src.width + font_w;
    modified.height = src.height * 2;
    modified.data = std::unique_ptr<bool[]>(
        new bool[modified.width * modified.height]);
    memset(modified.data.get(), 1, modified.width * modified.height);

    for (size_t y = 0; y < src.height; ++y)
    {
        for (size_t x = 0; x < 8; ++x)
        {
            for (size_t gx = 0; gx < font_w; ++gx)
            {
                if (src.get(x * font_w + gx, y) == 0)
                {
                    modified.set(x*(font_w+1) + gx  , y * 2, 0);
                    modified.set(x*(font_w+1) + gx+1, y * 2, 0);
                }
            }
        }
    }

    FILE *out = fopen("132col-normal.pbm", "w");
    write_pbm(out, modified);
    fclose(out);


    /* build the double-width font */
    Image dw{};
    dw.width = modified.width * 2;
    dw.height = modified.height;
    dw.data = std::unique_ptr<bool[]>(
        new bool[dw.width * dw.height]);
    memset(dw.data.get(), 1, dw.width * dw.height);

    for (size_t y = 0; y < src.height; ++y)
    {
        for (size_t x = 0; x < 8; ++x)
        {
            for (size_t gx = 0; gx < font_w; ++gx)
            {
                if (src.get(x * font_w + gx, y) == 0)
                {
                    dw.set(2*x*(font_w+1) + (2*gx)  , y * 2, 0);
                    dw.set(2*x*(font_w+1) + (2*gx)+1, y * 2, 0);
                    dw.set(2*x*(font_w+1) + (2*gx)+2, y * 2, 0);
                }
            }
        }
    }

    out = fopen("132col-doublewidth.pbm", "w");
    write_pbm(out, dw);
    fclose(out);


    /* build the double-height font */
    Image dh{};
    dh.width = dw.width;
    dh.height = dw.height * 2;
    dh.data = std::unique_ptr<bool[]>(
        new bool[dh.width * dh.height]);
    memset(dh.data.get(), 1, dh.width * dh.height);

    for (size_t y = 0; y < src.height; ++y)
    {
        for (size_t x = 0; x < 8; ++x)
        {
            for (size_t gx = 0; gx < font_w; ++gx)
            {
                if (src.get(x * font_w + gx, y) == 0)
                {
                    dh.set(2*x*(font_w+1) + (2*gx)  , y * 4, 0);
                    dh.set(2*x*(font_w+1) + (2*gx)+1, y * 4, 0);
                    dh.set(2*x*(font_w+1) + (2*gx)+2, y * 4, 0);

                    dh.set(2*x*(font_w+1) + (2*gx)  , y * 4 + 2, 0);
                    dh.set(2*x*(font_w+1) + (2*gx)+1, y * 4 + 2, 0);
                    dh.set(2*x*(font_w+1) + (2*gx)+2, y * 4 + 2, 0);
                }
            }
        }
    }

    out = fopen("132col-doubleheight.pbm", "w");
    write_pbm(out, dh);
    fclose(out);
    }
    /* ========== END OF 132 COLUMN FONTS ========== */


    return EXIT_SUCCESS;
}

