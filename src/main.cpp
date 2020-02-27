/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 * main.cpp
 *
 *  Terminal Emulator
 *
 */
/* TODO:
 *  local echo (half implemented)
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


std::unordered_map<SDL_Keycode, VT102::Key> const keymap =\
{
    /* SDL keycode  VT102 Key */
    { SDLK_F1,          VT102::Key::SetUp       },
    { SDLK_UP,          VT102::Key::Up          },
    { SDLK_DOWN,        VT102::Key::Down        },
    { SDLK_LEFT,        VT102::Key::Left        },
    { SDLK_RIGHT,       VT102::Key::Right       },

    { SDLK_ESCAPE,      VT102::Key::Escape      },
    { SDLK_1,           VT102::Key::KB_1        },
    { SDLK_2,           VT102::Key::KB_2        },
    { SDLK_3,           VT102::Key::KB_3        },
    { SDLK_4,           VT102::Key::KB_4        },
    { SDLK_5,           VT102::Key::KB_5        },
    { SDLK_6,           VT102::Key::KB_6        },
    { SDLK_7,           VT102::Key::KB_7        },
    { SDLK_8,           VT102::Key::KB_8        },
    { SDLK_9,           VT102::Key::KB_9        },
    { SDLK_0,           VT102::Key::KB_0        },
    { SDLK_MINUS,       VT102::Key::Minus       },
    { SDLK_EQUALS,      VT102::Key::Equals      },
    { SDLK_BACKQUOTE,   VT102::Key::Backtick    },
    { SDLK_BACKSPACE,   VT102::Key::Backspace   },
    { SDLK_PAUSE,       VT102::Key::Break       },

    { SDLK_TAB,         VT102::Key::Tab         },
    { SDLK_q,           VT102::Key::KB_Q        },
    { SDLK_w,           VT102::Key::KB_W        },
    { SDLK_e,           VT102::Key::KB_E        },
    { SDLK_r,           VT102::Key::KB_R        },
    { SDLK_t,           VT102::Key::KB_T        },
    { SDLK_y,           VT102::Key::KB_Y        },
    { SDLK_u,           VT102::Key::KB_U        },
    { SDLK_i,           VT102::Key::KB_I        },
    { SDLK_o,           VT102::Key::KB_O        },
    { SDLK_p,           VT102::Key::KB_P        },
    { SDLK_LEFTBRACKET, VT102::Key::LeftBracket },
    { SDLK_RIGHTBRACKET,VT102::Key::RightBracket},
    { SDLK_RETURN,      VT102::Key::Return      },
    { SDLK_DELETE,      VT102::Key::Delete      },

    { SDLK_a,           VT102::Key::KB_A        },
    { SDLK_s,           VT102::Key::KB_S        },
    { SDLK_d,           VT102::Key::KB_D        },
    { SDLK_f,           VT102::Key::KB_F        },
    { SDLK_g,           VT102::Key::KB_G        },
    { SDLK_h,           VT102::Key::KB_H        },
    { SDLK_j,           VT102::Key::KB_J        },
    { SDLK_k,           VT102::Key::KB_K        },
    { SDLK_l,           VT102::Key::KB_L        },
    { SDLK_SEMICOLON,   VT102::Key::Semicolon   },
    { SDLK_QUOTE,       VT102::Key::Quote       },
    { SDLK_BACKSLASH,   VT102::Key::Backslash   },

    { SDLK_SCROLLLOCK,  VT102::Key::NoScroll    },
    { SDLK_z,           VT102::Key::KB_Z        },
    { SDLK_x,           VT102::Key::KB_X        },
    { SDLK_c,           VT102::Key::KB_C        },
    { SDLK_v,           VT102::Key::KB_V        },
    { SDLK_b,           VT102::Key::KB_B        },
    { SDLK_n,           VT102::Key::KB_N        },
    { SDLK_m,           VT102::Key::KB_M        },
    { SDLK_COMMA,       VT102::Key::Comma       },
    { SDLK_PERIOD,      VT102::Key::Period      },
    { SDLK_SLASH,       VT102::Key::Slash       },
    { SDLK_LALT,        VT102::Key::LineFeed    },

    { SDLK_SPACE,       VT102::Key::Space       },

    { SDLK_HOME,        VT102::Key::PF1         },
    { SDLK_PAGEUP,      VT102::Key::PF2         },
    { SDLK_PAGEDOWN,    VT102::Key::PF3         },
    { SDLK_END,         VT102::Key::PF4         },

    { SDLK_KP_7,        VT102::Key::KP_7        },
    { SDLK_KP_8,        VT102::Key::KP_8        },
    { SDLK_KP_9,        VT102::Key::KP_9        },
    { SDLK_KP_MINUS,    VT102::Key::KP_Minus    },

    { SDLK_KP_4,        VT102::Key::KP_4        },
    { SDLK_KP_5,        VT102::Key::KP_5        },
    { SDLK_KP_6,        VT102::Key::KP_6        },
    { SDLK_KP_COMMA,    VT102::Key::KP_Comma    },

    { SDLK_KP_1,        VT102::Key::KP_1        },
    { SDLK_KP_2,        VT102::Key::KP_2        },
    { SDLK_KP_3,        VT102::Key::KP_3        },
    { SDLK_KP_ENTER,    VT102::Key::KP_Enter    },

    { SDLK_KP_0,        VT102::Key::KP_0        },
    { SDLK_KP_PERIOD,   VT102::Key::KP_Period   },
    /* KP_ENTER is not defined here */
};

Uint8 color_red = 255,
      color_grn = 255,
      color_blu = 255;




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


/* get the appropriate font */
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

/* get the appropriate palette */
std::unique_ptr<SDL_Color[]> get_palette(
    double brightness,
    bool inverted,
    bool bold)
{
    SDL_Color const colours[2] =\
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
            .r = (Uint8)(
                (bold? color_red : color_red * 0.75) * brightness),

            .g = (Uint8)(
                (bold? color_grn : color_grn * 0.75) * brightness),

            .b = (Uint8)(
                (bold? color_blu : color_blu * 0.75) * brightness),

            .a = 255
        }
    };

    std::unique_ptr<SDL_Color[]> palette(new SDL_Color[2]);
    /* background */
    palette[0] = colours[inverted? 1 : 0];
    /* foreground */
    palette[1] = colours[inverted? 0 : 1];

    return palette;
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
            get_palette(1, false, false).get(),
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
            auto pal = get_palette(
                term.setup.brightness,
                term.DECSCNM,
                false);
            SDL_FillRect(
                surf,
                nullptr,
                SDL_MapRGB(
                    surf->format,
                    pal[0].r,
                    pal[0].g,
                    pal[0].b));

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

                    auto pal = get_palette(
                        term.setup.brightness,
                        term.DECSCNM ^ ch.reverse,
                        ch.bold & ch.reverse);

                    SDL_SetPaletteColors(
                        glyph->format->palette,
                        pal.get(),
                        0,
                        2);

                    /* cursor is a blinking underline or block */
                    if (y == term.curs_y && x == term.curs_x)
                    {
                        ch.blink = true;
                        if (term.setup.block_cursor)
                        {
                            if (!blink_off)
                            {
                                ch.reverse = !ch.reverse;
                            }
                        }
                        else
                        {
                            ch.underline = !ch.underline;
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
                        auto rpal = get_palette(
                            term.setup.brightness,
                            term.DECSCNM ^ ch.reverse,
                            false);

                        /* fill the character area with the foreground
                         * colour, so the glyph is visible */
                        SDL_FillRect(
                            surf,
                            &scr_rect,
                            SDL_MapRGB(
                                surf->format,
                                rpal[1].r,
                                rpal[1].g,
                                rpal[1].b));
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
                            /* TODO: determine the underline
                             * position through the font? */
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
                                    pal[1].r,
                                    pal[1].g,
                                    pal[1].b));
                        }
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

        case SDL_KEYDOWN:
            if (!term.KAM)
            {
                if (   term.DECARM
                    || (!term.DECARM && event.key.repeat == 0))
                {
                    bool success = true;
                    /* check if the key is bound */
                    try
                    {
                        keymap.at(event.key.keysym.sym);
                    }
                    catch (std::out_of_range &e)
                    {
                        success = false;
                    }

                    /* if the key is bound, send the appropriate
                     * keypress event to the terminal */
                    if (success)
                    {
                        unsigned mod = VT102::Modifiers::None;
                        if (event.key.keysym.mod & KMOD_CTRL)
                        {
                            mod |= VT102::Modifiers::Ctrl;
                        }
                        if (event.key.keysym.mod & KMOD_SHIFT)
                        {
                            mod |= VT102::Modifiers::Shift;
                        }
                        if (event.key.keysym.mod & KMOD_CAPS)
                        {
                            mod |= VT102::Modifiers::CapsLock;
                        }
                        term.keyboard_input(
                            keymap.at(event.key.keysym.sym), mod);
                    }
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

