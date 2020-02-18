/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 * main.cpp
 *
 *  Terminal Emulator
 *
 */
/* TODO:
 *  auto repeat
 *  local echo
 *  SETUP mode
 *  rc file?
 */

/* for posix_openpt, ptsname, grantpt and unlockpt */
#define _XOPEN_SOURCE   600

#include "vt102.h"
#include "loadfont.h"

#include <SDL2/SDL.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>

#include <cstdlib>
#include <cstdbool>
#include <cctype>
#include <cstring>
#include <cstdio>

#include <vector>
#include <string>
#include <stdexcept>
#include <unordered_map>



typedef std::array<SDL_Surface *, 128> SurfaceFont;

std::string font_filenames[6] =\
{
    "font/80col-normal.pbm",
    "font/132col-normal.pbm",
    "font/80col-doublewidth.pbm",
    "font/132col-doublewidth.pbm",
    "font/80col-doubleheight.pbm",
    "font/132col-doubleheight.pbm",
};

SurfaceFont fonts[6];

enum class FontType
{
    Normal,
    DoubleWide,
    DoubleHigh,
};

SurfaceFont get_font(FontType type, bool use_132_columns)
{
    switch (type)
    {
    case FontType::Normal:
        return fonts[0 + use_132_columns];
        break;
    case FontType::DoubleWide:
        return fonts[2 + use_132_columns];
        break;
    case FontType::DoubleHigh:
        return fonts[4 + use_132_columns];
        break;
    }
    /* should never happen */
    throw std::runtime_error("something funky happened!");
}


std::unordered_map<SDL_Keycode, std::array<int, 3>> keymap =\
{
    /* SDL keycode
     *                 unshifted
     *                 |    shifted
     *                 |    |    ctrl
     *                 |    |    |
     *                 v    v    v */
    { SDLK_1,       { '1', '!',  -1 } },
    { SDLK_2,       { '2', '@',  -1 } },
    { SDLK_3,       { '3', '#',  -1 } },
    { SDLK_4,       { '4', '$',  -1 } },
    { SDLK_5,       { '5', '%',  -1 } },
    { SDLK_6,       { '6', '^',  -1 } },
    { SDLK_7,       { '7', '&',  -1 } },
    { SDLK_8,       { '8', '*',  -1 } },
    { SDLK_9,       { '9', '(',  -1 } },
    { SDLK_0,       { '0', ')',  -1 } },
    { SDLK_MINUS,   { '-', '_',  -1 } },
    { SDLK_EQUALS,  { '=', '+',  -1 } },
    { SDLK_BACKQUOTE,{ '`', '~', 036 } },
    { SDLK_q,       { 'q', 'Q', 021 } },
    { SDLK_w,       { 'w', 'W', 027 } },
    { SDLK_e,       { 'e', 'E', 005 } },
    { SDLK_r,       { 'r', 'R', 022 } },
    { SDLK_t,       { 't', 'T', 024 } },
    { SDLK_y,       { 'y', 'Y', 031 } },
    { SDLK_u,       { 'u', 'U', 025 } },
    { SDLK_i,       { 'i', 'I', 011 } },
    { SDLK_o,       { 'o', 'O', 017 } },
    { SDLK_p,       { 'p', 'P', 020 } },
    { SDLK_LEFTBRACKET,{ '[', '{', 033 } },
    { SDLK_RIGHTBRACKET,{ ']', '}', 035 } },
    { SDLK_DELETE,  { 127, 127, 127 } },
    { SDLK_a,       { 'a', 'A', 001 } },
    { SDLK_s,       { 's', 'S', 023 } },
    { SDLK_d,       { 'd', 'D', 004 } },
    { SDLK_f,       { 'f', 'F', 006 } },
    { SDLK_g,       { 'g', 'G', 007 } },
    { SDLK_h,       { 'h', 'H', 010 } },
    { SDLK_j,       { 'j', 'J', 012 } },
    { SDLK_k,       { 'k', 'K', 013 } },
    { SDLK_l,       { 'l', 'L', 014 } },
    { SDLK_SEMICOLON,{ ';', ':',  -1 } },
    { SDLK_QUOTE,   { '\'','"',  -1 } },
    { SDLK_BACKSLASH,{ '\\','|', 034 } },
    { SDLK_z,       { 'z', 'Z', 032 } },
    { SDLK_x,       { 'x', 'X', 030 } },
    { SDLK_c,       { 'c', 'C', 003 } },
    { SDLK_v,       { 'v', 'V', 026 } },
    { SDLK_b,       { 'b', 'B', 002 } },
    { SDLK_n,       { 'n', 'N', 016 } },
    { SDLK_m,       { 'm', 'M', 015 } },
    { SDLK_COMMA,   { ',', '<',  -1 } },
    { SDLK_PERIOD,  { '.', '>',  -1 } },
    { SDLK_SLASH,   { '/', '?', 037 } },
    { SDLK_SPACE,   { ' ', ' ', 000 } },

    /* for keypad keys, the second column is used for the final byte
     * of the code generated in ANSI alternate keypad mode */
    { SDLK_KP_0,    { '0', '0', -1 } },
    { SDLK_KP_1,    { '1', '1', -1 } },
    { SDLK_KP_2,    { '2', '2', -1 } },
    { SDLK_KP_3,    { '3', '3', -1 } },
    { SDLK_KP_4,    { '4', '4', -1 } },
    { SDLK_KP_5,    { '5', '5', -1 } },
    { SDLK_KP_6,    { '6', '6', -1 } },
    { SDLK_KP_7,    { '7', '7', -1 } },
    { SDLK_KP_8,    { '8', '8', -1 } },
    { SDLK_KP_9,    { '9', '9', -1 } },
    { SDLK_KP_MINUS,{ '-', '-', -1 } },
    { SDLK_KP_COMMA,{ ',', ',', -1 } },
    { SDLK_KP_PERIOD,{'.', '.', -1 } },
    /* KP_ENTER is not defined here */
    /* TODO: PF1, 2, 3, and 4 are unimplemented */
};



int colour_idx = 0;
int colour_isbold = 0;
SDL_Color colours[4][2] =\
{
    /* normal colours */
    {
        /* background */
        (SDL_Color)
        {
            .r =   0,
            .g =   0,
            .b =   0,
            .a = 255
        },
        /* foreground */
        (SDL_Color)
        {
            .r = (unsigned char)(colours[2][1].r * 0.75),
            .g = (unsigned char)(colours[2][1].g * 0.75),
            .b = (unsigned char)(colours[2][1].b * 0.75),
            .a = 255
        }
    },
    /* inverted colours */
    {
        /* background */
        colours[0][1],
        /* foreground */
        colours[0][0]
    },

    /* normal bold colours */
    {
        /* background */
        colours[0][0],
        /* foreground */
        (SDL_Color)
        {
            .r = 255,
            .g = 255,
            .b = 255,
            .a = 255
        }
    },
    /* inverted bold colours */
    {
        /* background */
        colours[0][1],
        /* foreground */
        colours[0][0]
    }
};



/* blink timer callback */
Uint32 callback_blink_timer(Uint32 interval, void *param)
{
    SDL_Event event;
    SDL_UserEvent userevent;

    userevent.type = SDL_USEREVENT;
    userevent.code = 0;
    userevent.data1 = nullptr;
    userevent.data2 = nullptr;

    event.type = SDL_USEREVENT;
    event.user = userevent;

    SDL_PushEvent(&event);

    return interval;
}

/* 60hz timer callback */
Uint32 callback_timer_60hz(Uint32 interval, void *param)
{
    SDL_Event event;
    SDL_UserEvent userevent;

    userevent.type = SDL_USEREVENT;
    userevent.code = 3;
    userevent.data1 = nullptr;
    userevent.data2 = nullptr;

    event.type = SDL_USEREVENT;
    event.user = userevent;

    SDL_PushEvent(&event);

    return interval;
}

/* fd read thread callback */
int thread_monitor_master_fd(void *data)
{
    int code = EXIT_SUCCESS;

    int fd = *((int *)data);

    struct pollfd fds{};
    fds.fd = fd;
    fds.events = POLLIN;

    for (bool done = false; !done;)
    {
        char ch = 0;

        int nfds = poll(&fds, 1, -1);
        if (nfds == -1 || nfds == 0)
        {
            perror("poll");
            code = EXIT_FAILURE;
            done = true;
            break;
        }

        ssize_t bytesread = read(fd, &ch, sizeof(ch));
        if (bytesread == -1)
        {
            int errno_backup = errno;
            /* child process quit */
            if (errno_backup == EIO)
            {
                done = true;
            }
            else
            {
                perror("read(master)");
                code = EXIT_FAILURE;
                done = true;
                break;
            }
        }
        else if (bytesread == 0)
        {
            done = true;
        }

        SDL_UserEvent userevent;
        userevent.type = SDL_USEREVENT;
        userevent.code = 1;
        userevent.data1 = (void *)new char(ch);
        userevent.data2 = nullptr;

        SDL_Event event;
        event.type = SDL_USEREVENT;
        event.user = userevent;

        SDL_PushEvent(&event);
    }

    /* tell the main thread that we're done */
    SDL_UserEvent userevent;
    userevent.type = SDL_USEREVENT;
    userevent.code = 2;
    userevent.data1 = nullptr;
    userevent.data2 = nullptr;

    SDL_Event event;
    event.type = SDL_USEREVENT;
    event.user = userevent;

    SDL_PushEvent(&event);

    return code;
}


/* convert an Image to an SDL_Surface */
SDL_Surface *image_to_surface(Image in)
{
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(
        0,
        in.width, in.height,
        8,
        SDL_PIXELFORMAT_INDEX8);

    memset(surf->pixels, 0, surf->pitch * surf->h);

    for (size_t y = 0; y < in.height; ++y)
    {
        for (size_t x = 0; x < in.width; ++x)
        {
            ((uint8_t *)surf->pixels)[(y * surf->pitch) + x] =\
                !in.get(x, y);
        }
    }

    SDL_FreePalette(surf->format->palette);

    surf->format->palette = SDL_AllocPalette(2);
    if (surf->format->palette == nullptr)
    {
        throw std::runtime_error(
            "failed to alloc palette "
            + std::string(SDL_GetError()));
    }

    if (SDL_SetPaletteColors(
            surf->format->palette,
            colours[0],
            0,
            2)
        < 0)
    {
        throw std::runtime_error(
            "failed to set palette colours "
            + std::string(SDL_GetError()));
    }

    return surf;
}

/* write to an fd */
void write_to(int fd, std::string data)
{
    char const *const cstr = data.c_str();
    size_t size = data.size();
    size_t bytes_total = 0;
    do
    {
        ssize_t tmp = write(fd, cstr, size - bytes_total);
        if (tmp == -1)
        {
            perror("write");
            exit(EXIT_FAILURE);
        }
        bytes_total += (size_t)tmp;
    } while (bytes_total < size);
}



/* Terminal Emulator */
int main (int argc, char *argv[])
{
    if (argc == 2)
    {
        VT102CONFIG_do_trace = (std::string(argv[1]) == "--trace");
    }

    int err = 0;

    /* open the pseudoterminal master fd */
    int master = posix_openpt(O_RDWR);
    if (master == -1)
    {
        perror("open(\"/dev/ptmx\")");
        exit(EXIT_FAILURE);
    }

    char const *const slave_filename = ptsname(master);
    if (slave_filename == nullptr)
    {
        perror("ptsname");
        exit(EXIT_FAILURE);
    }

    err = grantpt(master);
    if (err == -1)
    {
        perror("grantpt");
        exit(EXIT_FAILURE);
    }

    err = unlockpt(master);
    if (err == -1)
    {
        perror("unlockpt");
        exit(EXIT_FAILURE);
    }



    pid_t pid = fork();
    /* child */
    if (pid == 0)
    {
        close(master);

        int slave = open(slave_filename, O_RDWR);
        if (slave == -1)
        {
            perror("open(slave)");
            exit(EXIT_FAILURE);
        }

        /* create new session */
        setsid();

        /* make the tty a controlling tty of the calling process
         * (???) */
        if (ioctl(slave, TIOCSCTTY, nullptr) == -1)
        {
            perror("ioctl(TIOCSCTTY)");
            exit(EXIT_FAILURE);
        }

        /* rebind stdin, stdout, and stderr to slave */
        dup2(slave, 0);
        dup2(slave, 1);
        dup2(slave, 2);
        close(slave);

        if (putenv((char *)"TERM=vt102") != 0)
        {
            perror("putenv");
            exit(EXIT_FAILURE);
        }
        execl("/bin/bash", "/bin/bash", (char *)nullptr);
        perror("execle");
        exit(EXIT_FAILURE);
    }


    /* fork() parent */
    VT102 term{};

    /* init SDL2 */
    err = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    if (err < 0)
    {
        fprintf(stderr, "SDL_Init -- %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }


    /* load the font images */
    for (size_t i = 0; i < 6; ++i)
    {
        FILE *img = fopen(font_filenames[i].c_str(), "r");
        if (img == nullptr)
        {
            perror(("fopen(\"" + font_filenames[i] + "\")").c_str());
            exit(EXIT_FAILURE);
        }
        Font font_image = read_font(img);
        for (size_t j = 0; j < fonts[i].size(); ++j)
        {
            fonts[i][j] = image_to_surface(font_image[j]);
        }
        fclose(img);
    }


    /* open the SDL window */
    SDL_Window *win = SDL_CreateWindow(
        argv[0],
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        term.cols * fonts[0][0]->w,
        term.rows * fonts[0][0]->h,
        0/*SDL_WINDOW_RESIZABLE*/);

    SDL_Surface *surf = nullptr;


    SDL_TimerID blink_timer =\
        SDL_AddTimer(500, callback_blink_timer, nullptr);
    SDL_TimerID timer_60hz =\
        SDL_AddTimer(1000 / 60, callback_timer_60hz, nullptr);

    SDL_Thread *master_monitor = SDL_CreateThread(
        thread_monitor_master_fd,
        "master_monitor",
        &master);


    bool blink_off = false,
         /* !DECCOLM to make sure the window is the
          * right size before the first render */
         use_132_columns = !term.DECCOLM;

    /* mainloop */
    bool update_screen = true;
    for(bool done = false; !done;)
    {
        if (update_screen)
        {
            /* make sure the screen size is sync'd
             * with the emulator */
            if (term.DECCOLM != use_132_columns)
            {
                use_132_columns = term.DECCOLM;
                SurfaceFont fnt =\
                    get_font(FontType::Normal, use_132_columns);
                SDL_SetWindowSize(
                    win,
                    term.cols * fnt[0]->w,
                    term.rows * fnt[0]->h);
                surf = SDL_GetWindowSurface(win);
            }



            /* clear the screen */
            SDL_FillRect(
                surf,
                nullptr,
                SDL_MapRGB(
                    surf->format,
                    colours[term.DECSCNM][0].r,
                    colours[term.DECSCNM][0].g,
                    colours[term.DECSCNM][0].b));

            colour_idx = term.DECSCNM? 1 : 0;

            /* render the screen */
            for (ssize_t y = 0; y < term.rows; ++y)
            {
                Line line = term.screen[y];
                FontType font_type = FontType::Normal;
                switch (line.attr)
                {
                case Line::NORMAL:
                    font_type = FontType::Normal;
                    break;
                case Line::DOUBLE_HEIGHT_UPPER:
                case Line::DOUBLE_HEIGHT_LOWER:
                    font_type = FontType::DoubleHigh;
                    break;
                case Line::DOUBLE_WIDTH:
                    font_type = FontType::DoubleWide;
                    break;
                }

                SurfaceFont font = get_font(font_type, term.DECCOLM);

                for (ssize_t x = 0; x < term.cols; ++x)
                {
                    Char ch = term.getc_at(x, y);
                    int fontidx = term.fontidx(ch.charset, ch.ch);
                    SDL_Surface *glyph = font[fontidx];

                    colour_isbold = ch.bold? 2 : 0;

                    SDL_SetPaletteColors(
                        glyph->format->palette,
                        colours[colour_isbold + colour_idx],
                        0,
                        2);

                    /* cursor is a blinking underline or block */
                    if (y == term.curs_y && x == term.curs_x)
                    {
                        ch.blink = true;
                        if (term.block_cursor)
                        {
                            ch.reverse = true;
                        }
                        else
                        {
                            ch.underline = true;
                        }
                    }

                    SDL_Rect scr_rect;
                    scr_rect.w = glyph->w;
                    scr_rect.h = glyph->h;
                    scr_rect.x = x * glyph->w;
                    scr_rect.y = y * glyph->h;

                    SDL_Rect *src_rect = nullptr;

                    if (line.attr == Line::DOUBLE_WIDTH)
                    {
                        src_rect = new SDL_Rect{};
                        src_rect->x = 0;
                        src_rect->y = 0;
                        src_rect->w = glyph->w;
                        src_rect->h = glyph->h;
                    }
                    if (line.attr == Line::DOUBLE_HEIGHT_UPPER)
                    {
                        /* crop out the bottom half of the glyph */
                        src_rect = new SDL_Rect{};
                        src_rect->x = 0;
                        src_rect->y = 0;
                        src_rect->w = glyph->w;
                        src_rect->h = glyph->h / 2;

                        scr_rect.y = y * (glyph->h / 2);
                        scr_rect.h = glyph->h / 2;
                    }
                    if (line.attr == Line::DOUBLE_HEIGHT_LOWER)
                    {
                        /* crop out the top half of the glyph */
                        src_rect = new SDL_Rect{};
                        src_rect->x = 0;
                        src_rect->y = glyph->h / 2;
                        src_rect->w = glyph->w;
                        src_rect->h = glyph->h / 2;

                        scr_rect.y = y * (glyph->h / 2);
                        scr_rect.h = glyph->h / 2;
                    }

                    if (ch.reverse)
                    {
                        /* use inverted colours */
                        SDL_SetPaletteColors(
                            glyph->format->palette,
                            colours[colour_isbold + !colour_idx],
                            0,
                            2);

                        /* fill the character area with the foreground
                         * colour, so the glyph is visible */
                        SDL_FillRect(
                            surf,
                            &scr_rect,
                            SDL_MapRGB(
                                surf->format,
                                colours[colour_idx][1].r,
                                colours[colour_idx][1].g,
                                colours[colour_idx][1].b));
                    }

                    /* draw the character */
                    if (!ch.blink || !blink_off)
                    {
                        /* draw the glyph */
                        SDL_BlitSurface(
                            glyph,
                            src_rect,
                            surf,
                            &scr_rect);

                        if (ch.underline)
                        {
                            /* TODO: determine the underline position through the font? */
                            /* draw an underline */
                            SDL_Rect rect;
                            rect.w = scr_rect.w;
                            rect.h = 1;
                            rect.x = scr_rect.x;
                            rect.y = scr_rect.y + scr_rect.h - 2;

                            SDL_FillRect(
                                surf,
                                &rect,
                                SDL_MapRGB(
                                    surf->format,
                                    colours[colour_isbold + (ch.reverse? !colour_idx : colour_idx)][1].r,
                                    colours[colour_isbold + (ch.reverse? !colour_idx : colour_idx)][1].g,
                                    colours[colour_isbold + (ch.reverse? !colour_idx : colour_idx)][1].b));
                        }
                    }

                    if (ch.reverse)
                    {
                        /* use regular colours */
                        SDL_SetPaletteColors(
                            glyph->format->palette,
                            colours[colour_idx],
                            0,
                            2);
                    }
                    delete src_rect;
                }
            }
            SDL_UpdateWindowSurface(win);
            update_screen = false;
        }

        /* handle events */
        SDL_Event event;
        if (SDL_WaitEvent(&event) == 0)
        {
            done = true;
        }
        switch (event.type)
        {
        case SDL_QUIT:
            done = true;
            break;

        case SDL_WINDOWEVENT:
            switch (event.window.type)
            {
            case SDL_WINDOWEVENT_SHOWN:
            case SDL_WINDOWEVENT_EXPOSED:
            case SDL_WINDOWEVENT_RESIZED:
            case SDL_WINDOWEVENT_MAXIMIZED:
            case SDL_WINDOWEVENT_RESTORED:
                update_screen = true;
                break;

            case SDL_WINDOWEVENT_MOVED:
                puts("moved");
                break;
            }
            break;

        /* TODO: auto repeat, local echo */
        case SDL_KEYDOWN:
            if (!term.KAM)
            {
                switch (event.key.keysym.sym)
                {
                case SDLK_UP:
                case SDLK_DOWN:
                case SDLK_LEFT:
                case SDLK_RIGHT:
                  {
                    char msg[4] = "\0\0\0";
                    msg[0] = '\033';  /* ESC */
                    msg[1] = term.DECCKM? '0' : '[';
                    switch (event.key.keysym.sym)
                    {
                    case SDLK_UP:
                        msg[2] = 'A';
                        break;
                    case SDLK_DOWN:
                        msg[2] = 'B';
                        break;
                    case SDLK_RIGHT:
                        msg[2] = 'C';
                        break;
                    case SDLK_LEFT:
                        msg[2] = 'D';
                        break;
                    }
                    msg[3] = '\0';
                    write_to(master, std::string(msg));
                  } break;

                case SDLK_ESCAPE:
                  {
                    char ch = '\033'; /* ESC */
                    write(master, &ch, sizeof(ch));
                  } break;

                case SDLK_BACKSPACE:
                  {
                    char ch = '\b';
                    write(master, &ch, sizeof(ch));
                  } break;

                case SDLK_PAUSE:
                    /* TODO:
                     *  break defined by the computer system */
                    /* TODO: SHIFT + BREAK -> disconnect */
                    /* TODO:
                     *  CTRL + BREAK -> transmit answerback
                     *  message */
                    break;

                case SDLK_TAB:
                  {
                    char ch = '\t';
                    write(master, &ch, sizeof(ch));
                  } break;

                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    /* if the keypad is in application mode, KP_ENTER
                     * sends ESC 0 M */
                    if (   event.key.keysym.sym == SDLK_KP_ENTER
                        && term.keypad_mode != VT102::KPMode::Numeric)
                    {
                        write_to(master, "\0330M");
                    }
                    /* otherwise, KP_ENTER acts the same as RETURN */
                    else
                    {
                        char ch = '\r';
                        write(master, &ch, sizeof(ch));
                        if (term.LNM)
                        {
                            ch = '\n';
                            write(master, &ch, sizeof(ch));
                        }
                    }
                    break;

                case SDLK_KP_0:
                case SDLK_KP_1:
                case SDLK_KP_2:
                case SDLK_KP_3:
                case SDLK_KP_4:
                case SDLK_KP_5:
                case SDLK_KP_6:
                case SDLK_KP_7:
                case SDLK_KP_8:
                case SDLK_KP_9:
                case SDLK_KP_MINUS:
                case SDLK_KP_COMMA:
                case SDLK_KP_PERIOD:
                /* TODO: PF1, 2, 3, and 4 are unimplemented */
                    if (   term.keypad_mode
                        == VT102::KPMode::Application)
                    {
                        char code[4] =\
                        {
                            '\033', /* ESC */
                            '0',
                            (char)keymap.at(
                                event.key.keysym.sym).at(1),
                            '\0'
                        };
                        write_to(master, code);
                    }
                    else
                    {
                        char ch = keymap.at(
                            event.key.keysym.sym).at(0);
                        write(master, &ch, sizeof(ch));
                    }
                    break;


                default:
                  {
                    int idx = 0;
                    if (  event.key.keysym.mod
                        & (KMOD_SHIFT | KMOD_CAPS))
                    {
                        idx = 1;
                    }
                    /* IMPORTANT: CTRL overrides SHIFT! */
                    if (event.key.keysym.mod & KMOD_CTRL)
                    {
                        idx = 2;
                    }
                    bool do_write = true;
                    char ch = 0;
                    try
                    {
                        ch = keymap.at(
                            event.key.keysym.sym).at(idx);
                    }
                    catch (std::out_of_range &err)
                    {
                        do_write = false;
                    }
                    if (do_write)
                    {
                        write(master, &ch, sizeof(ch));
                    }
                  } break;
                }
            }
            break;

        case SDL_USEREVENT:
            switch (event.user.code)
            {
            /* blink notification */
            case 0:
                blink_off = !blink_off;
                break;
            /* byte received from the master fd */
            case 1:
                try
                {
                    term.interpret_byte(*(char *)event.user.data1);
                }
                catch (std::exception &e)
                {
                    printf("interpret_byte: %s\n", e.what());
                }
                delete (char *)event.user.data1;
                break;
            /* master fd disconnected */
            case 2:
                done = true;
                break;

            /* 60th of a second notification */
            case 3:
                update_screen = true;
                break;
            }
            break;
        }

        /* write any data from the terminal to the child */
        if (term.outbuffer.size() != 0)
        {
            printf("outbuffer '");
            for (char ch : term.outbuffer)
            {
                if (isprint(ch))
                {
                    putchar(ch);
                }
                else
                {
                    printf("^%c", '@' + ch);
                }
            }
            printf("'\n");
            write_to(master, term.outbuffer);
            term.outbuffer.erase();
        }
    }


    kill(pid, SIGKILL);
    int code = 0;
    SDL_WaitThread(master_monitor, &code);
    printf("master_monitor: %d\n", code);
    close(master);

    SDL_RemoveTimer(blink_timer);
    SDL_RemoveTimer(timer_60hz);

    for (SurfaceFont font : fonts)
    {
        for (SDL_Surface *s : font)
        {
            SDL_FreeSurface(s);
        }
    }
    SDL_DestroyWindow(win);
    SDL_Quit();

    return EXIT_SUCCESS;
}

