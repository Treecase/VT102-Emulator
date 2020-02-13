/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 * vt102.cpp
 *
 *  VT102 Emulator
 *
 */
/* TODO:
 *  figure out correct cursor movements (see vttest 8)
 */

#include "vt102.h"

#include <cstdlib>
#include <cstring>

#include <stdexcept>



void VT102::output(std::string message)
{
    if (xon)
    {
        outbuffer += message;
    }
    else
    {
        for (char ch : message)
        {
            /* while XOFF, the terminal only transmits XON/XOFF */
            if (ch == 0x11 || ch == 0x13)
            {
                outbuffer += ch;
            }
        }
    }
}


void VT102::interpret_byte(uint8_t ch)
{
    switch (state)
    {
    case State::Normal:
        interpret_byte_control_character(ch);
        break;


    case State::Escape:
        interpret_byte_escape(ch);
        break;


    case State::CtrlSeq:
        interpret_byte_ctrlseq(ch);
        break;

    case State::Pound:
        switch (ch)
        {
        /* DECDHL: upper half double-height double-width */
        case '3':
            screen[curs_y].attr = Line::DOUBLE_HEIGHT_UPPER;
            break;

        /* DECDHL: lower half double-height double-width */
        case '4':
            screen[curs_y].attr = Line::DOUBLE_HEIGHT_LOWER;
            break;

        /* DECSWL: single-height single-width */
        case '5':
            screen[curs_y].attr = Line::NORMAL;
            break;

        /* DECDWL: single-height double-width */
        case '6':
            screen[curs_y].attr = Line::DOUBLE_WIDTH;
            break;

        /* DECALN */
        case '8':
            for (ssize_t y = 0; y < rows; ++y)
            {
                for (ssize_t x = 0; x < cols; ++x)
                {
                    curs_x = x;
                    curs_y = y;
                    this->putc('E');
                }
            }
            break;


        default:
            throw std::runtime_error(
                "undefined escape sequence `ESC # "
                + std::string(1, ch)
                + "`");
        }
        state = State::Normal;
        break;

    case State::G0SetSelect:
        switch (ch)
        {
        case 'A':
            g[0] = CharSet::UnitedKingdom;
            break;
        case 'B':
            g[0] = CharSet::UnitedStates;
            break;
        case '0':
            g[0] = CharSet::Special;
            break;
        case '1':
            g[0] = CharSet::AltROM;
            break;
        case '2':
            g[0] = CharSet::AltROMSpecial;
            break;

        default:
            throw std::runtime_error(
                "undefined escape sequence `ESC ( "
                + std::to_string(ch)
                //+ std::string(1, ch)
                + "`");
            break;
        }
        state = State::Normal;
        break;

    case State::G1SetSelect:
        switch (ch)
        {
        case 'A':
            g[1] = CharSet::UnitedKingdom;
            break;
        case 'B':
            g[1] = CharSet::UnitedStates;
            break;
        case '0':
            g[1] = CharSet::Special;
            break;
        case '1':
            g[1] = CharSet::AltROM;
            break;
        case '2':
            g[1] = CharSet::AltROMSpecial;
            break;

        default:
            throw std::runtime_error(
                "undefined escape sequence `ESC ) "
                + std::string(1, ch)
                + "`");
            break;
        }
        state = State::Normal;
        break;
    }
}

void VT102::interpret_byte_control_character(uint8_t ch)
{
    switch (ch)
    {
    /* NUL */
    case '\000':
        /* ignored */
        break;

    /* ETX */
    case '\003':
        /* TODO: selectable as half-duplex turnaround */
        throw std::runtime_error("ETX not implemented");
        break;

    /* EOT */
    case '\004':
        /* TODO: selectable as half-duplex turnaround or disconnect */
        throw std::runtime_error("EOT not implemented");
        break;

    /* ENQ */
    case '\005':
        output(answerback);
        break;

    /* BEL */
    case '\a':
        /* TODO: beep */
        puts("boop");
        break;

    /* BS */
    case '\b':
        if (curs_x - 1 >= 0)
        {
            curs_x -= 1;
        }
        break;

    /* HT */
    case '\t':
      {
        ssize_t tmp = curs_x;
        /* HT moves the cursor to the next tab stop,
         * or to the right margin if there are no more tab stops */
        curs_x = cols - 1;
        for (ssize_t x = tmp + 1; x < cols; ++x)
        {
            if (tab_stops[x])
            {
                curs_x = x;
                break;
            }
        }
      } break;

    /* LF, VT, FF */
    case '\n':
    case '\v':
    case '\f':
        /* TODO: proper movement (scrolling, etc.) */
        /* if LNM is set, LF moves to the next line AND
         * moves to column 0 */
        if (LNM)
        {
            move_curs(0, curs_y + 1);
        }
        /* otherwise, LF just moves to the next line */
        else
        {
            move_curs(curs_x, curs_y + 1);
        }
        if (ch == '\f')
        {
            /* TODO: can be selected as half-duplex turnaround */
        }
        break;

    /* CR */
    case '\r':
        curs_x = 0;
        /* TODO: can be selected as half-duplex turnaround */
        break;

    /* SO */
    case '\016':
        current_charset = 1;
        break;

    /* SI */
    case '\017':
        current_charset = 0;
        break;

    /* DC1 */
    case '\021':
        xon = true;
        break;

    /* DC3 */
    case '\023':
        /* TODO: can be selected as half-duplex turnaround */
        xon = false;
        break;

    /* CAN, SUB */
    case '\030':
    case '\032':
        if (state == State::Escape || state == State::CtrlSeq)
        {
            state = State::Normal;
            /* CAN and SUB display a substitution character
             * when they cancel a sequence */
            this->putc(0x1A);
        }
        break;

    /* ESC */
    case '\033':
        state = State::Escape;
        break;

    /* DEL */
    case '\177':
        /* ignored */
        puts("del");    /* FIXME: TEMP */
        break;


    /* normal character */
    default:
        this->putc(ch);
        break;
    }
}

void VT102::interpret_byte_escape(uint8_t ch)
{
    state = State::Normal;
    switch (ch)
    {
    /* RIS */
    case 'c':
        /* TODO: reset (leave this unimplemented?) */
        throw std::runtime_error("RIS not implemented");
        break;

    /* IND */
    case 'D':
        curs_y += 1;
        if (curs_y > scroll_bottom)
        {
            scroll(-1);
        }
        break;

    /* NEL */
    case 'E':
        curs_x = 0;
        curs_y += 1;
        if (curs_y > scroll_bottom)
        {
            scroll(-1);
        }
        break;

    /* HTS */
    case 'H':
        tab_stops[curs_x] = true;
        break;

    /* RI */
    case 'M':
        curs_y -= 1;
        if (curs_y < scroll_top)
        {
            scroll(+1);
        }
        break;

    /* SS2 */
    case 'N':
        single_shift = 2;
        break;

    /* DECID */
    case 'Z':
        output("\033[?6c");
        break;

    /* SS3 */
    case '0':
        single_shift = 3;
        break;

    /* DECSC */
    case '7':
        /* save cursor position, character attribute, charset,
         * and origin mode */
        if (saved == nullptr)
        {
            saved = new SavedData{};
        }
        saved->x = curs_x;
        saved->y = curs_y;
        saved->charattr = char_attributes;
        saved->charset =\
            (single_shift == -1)\
                ? current_charset
                : single_shift;
        saved->DECOM = DECOM;
        break;

    /* DECRC */
    case '8':
        /* restore previously saved state, or reset cursor to home
         * position if there is no saved state */
        if (saved == nullptr)
        {
            curs_x = 0;
            curs_y = 0;
        }
        else
        {
            /* TODO: should the saved data be deleted? */
            curs_x = saved->x;
            curs_y = saved->y;
            DECOM = saved->DECOM;
            char_attributes = saved->charattr;
            current_charset = saved->charset;
        }
        break;

    /* CSI */
    case '[':
        cmd = new ControlSequence();
        state = State::CtrlSeq;
        break;

    /* line attributes and screen alignment test introducer */
    case '#':
        state = State::Pound;
        break;

    /* G0 character set selection introducer */
    case '(':
        state = State::G0SetSelect;
        break;

    /* G1 character set selection introducer */
    case ')':
        state = State::G1SetSelect;
        break;

    /* DECKPNM */
    case '>':
        keypad_mode = KPMode::Numeric;
        break;

    /* DECKPAM */
    case '=':
        keypad_mode = KPMode::Application;
        break;

    default:
        throw std::runtime_error(
            "undefined escape sequence `ESC "
            //+ std::string(1, ch)
            + std::to_string(ch)
            + "`");
        break;
    }
}

void VT102::interpret_byte_ctrlseq(uint8_t ch)
{
    switch (ch)
    {
    /* Intermediate Byte */
    case 0x20 ... 0x2F:
        cmd->intermediate += ch;
        break;

    /* Parameter Byte */
    case 0x30 ... 0x3F:
        /* first byte */
        if (cmd->params.size() == 0)
        {
            /* If a separator is the first byte, then an
             * empty parameter is assumed before it */
            if (ch == ';')
            {
                cmd->params.push_back(std::string(""));
            }
            else
            {
                cmd->params.push_back(std::string(1, ch));
                if (ch == '?')
                {
                    cmd->params.push_back(std::string(""));
                }
            }
        }
        else
        {
            if (ch == ';')
            {
                cmd->params.push_back(std::string(""));
            }
            else
            {
                cmd->params.back() += ch;
            }
        }
        break;

    /* Final Byte */
    case 0x40 ... 0x7E:
        state = State::Normal;
        cmd->final = ch;
        /* ECMA-48 only defines control sequences with
         * either 1 or 0 intermediate bytes */
        if (cmd->intermediate.size() == 0)
        {
            switch (cmd->final)
            {
            /* CUU */
            case 'A':
              {
                int delta = 1;
                switch (cmd->params.size())
                {
                case 0:
                    delta = 1;
                    break;
                case 1:
                    delta = std::stoi(cmd->params[0]);
                    break;
                default:
                    throw std::runtime_error(
                        "CUU takes up to 1 parameter");
                    break;
                }
                if (curs_y - delta < scroll_top)
                {
                    delta = curs_y - scroll_top;
                }
                move_curs(curs_x, curs_y - delta);
              } break;

            /* CUD */
            case 'B':
              {
                int delta = 1;
                switch (cmd->params.size())
                {
                case 0:
                    delta = 1;
                    break;
                case 1:
                    delta = std::stoi(cmd->params[0]);
                    break;
                default:
                    throw std::runtime_error(
                        "CUD takes up to 1 parameter");
                    break;
                }
                if (curs_y + delta > scroll_bottom)
                {
                    delta = scroll_bottom - curs_y;
                }
                move_curs(curs_x, curs_y + delta);
              } break;

            /* CUF */
            case 'C':
              {
                int delta = 1;
                switch (cmd->params.size())
                {
                case 0:
                    delta = 1;
                    break;
                case 1:
                    delta = std::stoi(cmd->params[0]);
                    break;
                default:
                    throw std::runtime_error(
                        "CUF takes up to 1 parameter");
                    break;
                }
                if (curs_x + delta >= cols)
                {
                    delta = (curs_x + delta) - (cols - 1);
                }
                move_curs(curs_x + delta, curs_y);
              } break;

            /* CUB */
            case 'D':
              {
                int delta = 1;
                switch (cmd->params.size())
                {
                case 0:
                    delta = 1;
                    break;
                case 1:
                    delta = std::stoi(cmd->params[0]);
                    break;
                default:
                    throw std::runtime_error(
                        "CUB takes up to 1 parameter");
                    break;
                }
                if (curs_x - delta < 0)
                {
                    delta = curs_x;
                }
                move_curs(curs_x - delta, curs_y);
              } break;

            /* CUP, HVP */
            case 'H':
            case 'f':
              {
                int newx = 0,
                    newy = 0;
                switch (cmd->params.size())
                {
                case 0:
                    newx = 0;
                    newy = 0;
                    break;
                case 2:
                    if (cmd->params[1] != "")
                    {
                        newx = std::stoi(cmd->params[1]) - 1;
                    }
                    else
                    {
                        newx = 0;
                    }
                /* intentional fallthrough */
                case 1:
                    if (cmd->params[0] != "")
                    {
                        newy = std::stoi(cmd->params[0]) - 1;
                    }
                    else
                    {
                        newy = 0;
                    }
                    break;

                default:
                    throw std::runtime_error(
                        std::string((ch == 'H')? "CUP" : "HVP")
                        + "takes up to 2 parameters");
                    break;
                }
                /* IMPORTANT:
                 *  move_curs is not used here intentionally,
                 *  because CUP and HVP allow the cursor to be
                 *  moved outside of the screen. */
                if (DECOM)
                {
                    curs_x = newx;
                    curs_y = scroll_top + newy;
                    /* if DECOM is set, the cursor cannot move
                     * outside of the scrolling region */
                    if (curs_y < scroll_top)
                    {
                        curs_y = scroll_top;
                    }
                    else if (curs_y > scroll_bottom)
                    {
                        curs_y = scroll_bottom;
                    }
                }
                else
                {
                    curs_x = newx;
                    curs_y = newy;
                }
                /* x locking is not dependant on DECOM setting(?) */
                if (curs_x < 0)
                {
                    curs_x = 0;
                }
                else if (curs_x >= cols)
                {
                    curs_x = cols - 1;
                }
              } break;

            /* ED */
            case 'J':
              {
                int arg = 0;
                switch (cmd->params.size())
                {
                case 0:
                    arg = 0;
                    break;
                case 1:
                    arg = std::stoi(cmd->params[0]);
                    break;

                default:
                    throw std::runtime_error("ED takes up to 1 parameter");
                    break;
                }
                switch (arg)
                {
                case 0:
                    /* erase from cursor to end of screen */
                    for (ssize_t y = curs_y; y < rows; ++y)
                    {
                        for (ssize_t x = curs_x; x < cols; ++x)
                        {
                            erase(x, y);
                        }
                        screen[y].attr = Line::NORMAL;
                    }
                    break;
                case 1:
                    /* erase from start of screen to cursor */
                    for (ssize_t y = 0; y <= curs_y; ++y)
                    {
                        for (ssize_t x = 0; x <= curs_x; ++x)
                        {
                            erase(x, y);
                        }
                        screen[y].attr = Line::NORMAL;
                    }
                    break;
                case 2:
                    /* erase entire display */
                    for (ssize_t y = 0; y < rows; ++y)
                    {
                        for (ssize_t x = 0; x < cols; ++x)
                        {
                            erase(x, y);
                        }
                        screen[y].attr = Line::NORMAL;
                    }
                    break;

                default:
                    throw std::runtime_error("ED only accepts 1,2, or 3 as a parameter");
                    break;
                }
              } break;

            /* EL */
            case 'K':
              {
                int arg = 0;
                switch (cmd->params.size())
                {
                case 0:
                    arg = 0;
                    break;
                case 1:
                    arg = std::stoi(cmd->params[0]);
                    break;

                default:
                    throw std::runtime_error("EL takes up to 1 parameter");
                    break;
                }
                switch (arg)
                {
                case 0:
                    /* erase from cursor to end of line */
                    for (ssize_t i = curs_x; i < cols; ++i)
                    {
                        erase(i, curs_y);
                    }
                    break;
                case 1:
                    /* erase from start to cursor */
                    for (ssize_t i = 0; i <= curs_x; ++i)
                    {
                        erase(i, curs_y);
                    }
                    break;
                case 2:
                    /* erase entire line */
                    for (ssize_t i = 0; i < cols; ++i)
                    {
                        erase(i, curs_y);
                    }
                    break;

                default:
                    throw std::runtime_error("EL only accepts 0, 1 or 2 as a parameter");
                    break;
                }
              } break;

            /* IL */
            case 'L':
              {
                /* insert N blank lines (default 1) */
                int arg = 0;
                switch (cmd->params.size())
                {
                case 0:
                    arg = 1;
                    break;
                case 1:
                    arg = std::stoi(cmd->params[0]);
                    break;

                default:
                    throw std::runtime_error(
                        "IL takes up to 1 parameter");
                    break;
                }
                /* this sequence is ignored when the cursor is
                 * outside the scrolling region */
                if (scroll_top <= curs_y && curs_y <= scroll_bottom)
                {
                    for (int i = 0; i < arg; ++i)
                    {
                        ins_line(curs_y);
                    }
                }
              } break;

            /* DL */
            case 'M':
              {
                /* delete N lines (default 1) */
                int arg = 0;
                switch (cmd->params.size())
                {
                case 0:
                    arg = 1;
                    break;
                case 1:
                    arg = std::stoi(cmd->params[0]);
                    break;

                default:
                    throw std::runtime_error(
                        "DL takes up to 1 parameter");
                    break;
                }
                /* this sequence is ignored when the cursor is
                 * outside the scrolling region */
                if (scroll_top <= curs_y && curs_y <= scroll_bottom)
                {
                    for (int i = 0; i < arg; ++i)
                    {
                        del_line(curs_y);
                    }
                }
              } break;

            /* DCH */
            case 'P':
              {
                /* delete N characters */
                int arg = 0;
                switch (cmd->params.size())
                {
                case 0:
                    arg = 1;
                    break;
                case 1:
                    arg = std::stoi(cmd->params[0]);
                    break;

                default:
                    throw std::runtime_error(
                        "DCH takes up to 1 parameter");
                    break;
                }
                for (int i = 0; i < arg; ++i)
                {
                    del_char(curs_x, curs_y);
                }
              } break;

            /* DA */
            case 'c':
                output("\033[?6c");
                break;

            /* TBC */
            case 'g':
                switch (cmd->params.size())
                {
                /* clear tab stop at current position */
                case 0:
                    tab_stops[curs_x] = false;
                    break;
                /* clear all tab stops */
                case 1:
                    if (cmd->params[0] == "0")
                    {
                        tab_stops[curs_x] = false;
                    }
                    else if (cmd->params[0] == "3")
                    {
                        for (size_t x = 0; x < tab_stops.size(); ++x)
                        {
                            tab_stops[x] = false;
                        }
                        break;
                    }
                    else
                    {
                        /* TBC ignores undefined parameters */
                        //throw std::runtime_error(
                        //    "TBC received undefined parameter "
                        //    + cmd->params[0]);
                    }
                    break;

                default:
                    throw std::runtime_error(
                        "TBC takes up to 1 parameter");
                    break;
                }
                break;

            /* SM, RM */
            case 'h':
            case 'l':
              {
                bool setting = cmd->final == 'h';
                if (cmd->params.size() == 1)
                {
                    int mode = std::stoi(cmd->params[0]);
                    switch (mode)
                    {
                    case 2:
                        KAM = setting;
                        break;
                    case 4:
                        IRM = setting;
                        break;
                    case 12:
                        SRM = setting;
                        break;
                    case 20:
                        LNM = setting;
                        break;

                    default:
                        throw std::runtime_error(
                            std::string(setting? "S" : "R")
                            + std::string(
                                "M received undefined parameter ")
                            + cmd->params[0]);
                        break;
                    }
                }
                else if (cmd->params.size() == 2)
                {
                    if (cmd->params[0] == "?")
                    {
                        int mode = std::stoi(cmd->params[1]);
                        switch (mode)
                        {
                        case 1:
                            /* when the keypad is in Numeric mode,
                             * DECCKM is always reset */
                            if (keypad_mode == KPMode::Numeric)
                            {
                                DECCKM = false;
                            }
                            else
                            {
                                DECCKM = setting;
                            }
                            break;
                        case 2:
                            if (!setting)
                            {
                                DECANM = true;
                                throw std::runtime_error(
                                    "VT52 compatibility mode"
                                    "unimplemented!");
                            }
                            break;
                        case 3:
                            DECCOLM = setting;
                            cols = setting? 132 : 80;
                            /* when the columns per line is changed,
                             * the screen is erased */
                            for (ssize_t y = 0; y < rows; ++y)
                            {
                                for (ssize_t x = 0; x < cols; ++x)
                                {
                                    erase(x, y);
                                }
                            }
                            break;
                        case 4:
                            DECSCLM = setting;
                            break;
                        case 5:
                            DECSCNM = setting;
                            break;
                        case 6:
                            DECOM = setting;
                            /* the cursor moves to the new home
                             * position when DECOM is changed */
                            move_curs(0, DECOM? scroll_top : 0);
                            break;
                        case 7:
                            DECAWM = setting;
                            break;
                        case 8:
                            DECARM = setting;
                            break;
                        case 18:
                            DECPFF = setting;
                            break;
                        case 19:
                            DECPEX = setting;
                            break;

                        case 40:
                        case 42:
                        case 45:
                            /* ??? */
                            break;

                        default:
                            throw std::runtime_error(
                                std::string(setting? "S" : "R")
                                + std::string("M - undefined DEC Private Mode sequence")
                                + cmd->params[1]);
                            break;
                        }
                    }
                }
                else
                {
                    throw std::runtime_error(
                        std::string(setting? "S" : "R")
                        + std::string("M takes 1 or 2 parameters"));
                }
              } break;

            /* MC */
            case 'i':
                /* ignored by this emulator */
                break;

            /* SGR */
            case 'm':
                if (cmd->params.size() == 0)
                {
                    char_attributes = 0;
                }
                else
                {
                    for (std::string param : cmd->params)
                    {
                        int attr = ((param == "")? 0 : std::stoi(param));
                        switch (attr)
                        {
                        case 0:
                            char_attributes = 0;
                            break;
                        case 1:
                            char_attributes |= BOLD;
                            break;
                        case 4:
                            char_attributes |= UNDERLINE;
                            break;
                        case 5:
                            char_attributes |= BLINK;
                            break;
                        case 7:
                            char_attributes |= REVERSE;
                            break;
                        }
                    }
                }
                break;

            /* DSR */
            case 'n':
                if (cmd->params.size() != 1)
                {
                    throw std::runtime_error("DSR takes 1 parameter");
                }
                else
                {
                    if (cmd->params.size() == 2)
                    {
                        if (cmd->params[0] == "?")
                        {
                            int code = std::stoi(cmd->params[1]);
                            if (code == 15)
                            {
                                /*  `ESC [ ? 13 n` - no printer connected
                                 *  `ESC [ ? 11 n` - printer not ready
                                 *  `ESC [ ? 10 n` - printer ready */
                                output("\033[?13n");
                            }
                        }
                        else
                        {
                            throw std::runtime_error(
                                "undefined DEC Private Mode sequence `ESC [ "
                                + cmd->params[0]
                                + ";"
                                + cmd->params[1]
                                + "n`");
                        }
                    }
                    else if (cmd->params.size() == 1)
                    {
                        int code = std::stoi(cmd->params[0]);
                        switch (code)
                        {
                        /* status report */
                        case 5:
                            /* `ESC [ 0 n` - ready, no errors
                             * `ESC [ 3 n` - error */
                            output("\033[0n");
                            break;
                        case 6:
                            /* `ESC [ curs_y ; curs_x R` */
                            output(
                                "\033["
                                + std::to_string(scroll_top + curs_y + 1)
                                + ";"
                                + std::to_string(curs_x + 1)
                                + "R");
                            break;
                        }
                    }
                    else
                    {
                        throw std::runtime_error("DSR takes 1 or 2 arguments");
                    }
                }
                break;

            /* DECLL */
            case 'q':
                /* TODO: */
                if (cmd->params.size() == 1)
                {
                    /* LED on */
                    if (cmd->params[0] == "0")
                    {
                    }
                    /* LED off */
                    else if (cmd->params[0] == "1")
                    {
                    }
                    else
                    {
                        throw std::runtime_error(
                            "invalid argument to DECLL");
                    }
                }
                else
                {
                    throw std::runtime_error(
                        "DECLL takes 1 argument");
                }
                break;

            /* DECSTBM */
            case 'r':
              {
                int top = 0,
                    bottom = rows-1;
                switch (cmd->params.size())
                {
                case 0:
                    break;
                case 2:
                    bottom = std::stoi(cmd->params[1])-1;
                    /* intentional fallthrough! */
                case 1:
                    top = std::stoi(cmd->params[0])-1;
                    break;

                default:
                    throw std::runtime_error(
                        "DECSTBM takes up to 2 arguments");
                    break;
                }
                /* minimum size of the scrolling region is 2 lines */
                if (top < bottom && top >= 0 && bottom < rows)
                {
                    scroll_top = top;
                    scroll_bottom = bottom;
                    /* after the margins are selected,
                     * the cursor moves to the home position */
                    move_curs(0, DECOM? scroll_top : 0);
                }
              } break;

            /* DECTST */
            case 'y':
                throw std::runtime_error("DECTST not implemented");
                break;


            default:
              {
                std::string errmsg =\
                    "undefined control sequence ESC [";
                for (std::string param : cmd->params)
                {
                    for (char byte : param)
                    {
                        errmsg += " " + std::to_string(byte);
                    }
                    errmsg += " ;";
                }
                for (char byte : cmd->intermediate)
                {
                    errmsg += " " + std::to_string(byte);
                }
                errmsg += " " + std::to_string(cmd->final);
                throw std::runtime_error(errmsg);
              } break;
            }
        }
        else
        {
            std::string errmsg = "undefined control sequence ESC [";
            for (char byte : cmd->intermediate)
            {
                errmsg += " " + std::to_string(byte);
            }
            errmsg += " " + std::to_string(cmd->final);
            throw std::runtime_error(errmsg);
        }
        delete cmd;
        cmd = nullptr;
        break;


    default:
        interpret_byte_control_character(ch);
        break;
    }
}


Char VT102::getc_at(ssize_t x, ssize_t y) const
{
    if (    cols <= x || x < 0
        ||  rows <= y || y < 0)
    {
        throw std::invalid_argument(
            std::string((cols <= x || x < 0)? "x" : "y")
            + " out of range");
    }
    else
    {
        return screen[y][x];
    }
}

size_t VT102::fontidx(CharSet charset, unsigned char ch)
{
    size_t idx = 0;
    switch (charset)
    {
    /* the only difference between the US and UK charsets are for '#',
     * which renders as '#' in the US, and as the pound (currency)
     * symbol in the UK */
    case CharSet::UnitedStates:
    case CharSet::UnitedKingdom:
        switch (ch)
        {
        /* 1st and 2nd columns are control characters, so they don't
         * render as anything, except for SUB, which draws the
         * 'substitute character' */
        case 0x1A:
            idx = 16;
            break;

        case '#':
            if (charset == CharSet::UnitedStates)
            {
                idx = 26;
            }
            else    /* UK */
            {
                idx = 113;
            }
            break;

        default:
            idx = (8 * ch) % 127;
            break;
        }
        break;

    case CharSet::Special:
        switch (ch)
        {
        /* 1st and 2nd columns are control characters, so they don't
         * render as anything, except for SUB, which draws the
         * 'substitute character' */
        case 0x1A:
            idx = 16;
            break;

        case '_':
            idx = 0;
            break;

        /* 7th column */
        case '`':
        case 'a' ... 'o':
            idx = (8 + (8 * (ch - '`'))) % 127;
            break;

        /* 8th column */
        case 'p' ... 'z':
        case '{':
        case '|':
        case '}':
        case '~':
            idx = (9 + (8 * (ch - 'p'))) % 127;
            break;

        default:
            idx = (8 * ch) % 127;
            break;
        }
        break;

    case CharSet::AltROM:
    case CharSet::AltROMSpecial:
        idx = (8 * ch) % 127;
        break;
    }
    return idx;
}

void VT102::erase(ssize_t x, ssize_t y)
{
    if (    x >= 0 && x < cols
        &&  y >= 0 && y < rows)
    {
        Line &line = screen.at(y);
        line.chars.at(x).ch        = ' ';
        line.chars.at(x).underline = false;
        line.chars.at(x).reverse   = false;
        line.chars.at(x).blink     = false;
        line.chars.at(x).bold      = false;
        line.chars.at(x).charset   = g[0];
    }
}

void VT102::del_char(ssize_t x, ssize_t y)
{
    Line &line = screen[y];

    for (ssize_t i = x; i < cols-1; ++i)
    {
        line[i] = line[i + 1];
    }
    /* character attributes ARE NOT modified */
    line[cols-1].ch = ' ';
    line[cols-1].charset = g[current_charset];
}

void VT102::del_line(ssize_t y)
{
    /* lines below the cursor move up */
    for (size_t i = y; i < screen.size()-1; ++i)
    {
        screen[i] = screen[i + 1];
    }
    for (Char &chr : screen[screen.size()-1].chars)
    {
        /* character attributes ARE NOT modified */
        chr.ch = ' ';
        chr.charset = g[current_charset];
    }
}

void VT102::ins_line(ssize_t y)
{
    /* lines below the cursor move down */
    for (ssize_t i = screen.size()-2; i >= y; --i)
    {
        screen[i + 1] = screen[i];
    }
    /* clear the inserted line */
    for (size_t x = 0; x < screen[y].chars.size(); ++x)
    {
        erase(x, y);
    }
    screen[y].attr = Line::NORMAL;
}

void VT102::putc(unsigned char ch)
{
    if (    0 <= curs_x && curs_x < cols
        &&  0 <= curs_y && curs_y < rows)
    {
        /* if IRM is set, added characters move previously displayed
         * characters 1 position to the right */
        if (IRM)
        {
            for (ssize_t i = cols-2; i >= curs_x; --i)
            {
                screen[curs_y][i + 1] = screen[curs_y][i];
            }
        }

        /* erase the old character */
        erase(curs_x, curs_y);

        /* add the new character */
        Char &chr = screen[curs_y][curs_x];
        chr.ch = ch;

        /* SS2 and SS3 */
        if (single_shift != -1)
        {
            chr.charset = g[single_shift];
            single_shift = -1;
        }
        else
        {
            chr.charset = g[current_charset];
        }

        chr.bold = char_attributes & BOLD;
        chr.underline = char_attributes & UNDERLINE;
        chr.blink = char_attributes & BLINK;
        chr.reverse = char_attributes & REVERSE;

        /* move the cursor */
        if (curs_x + 1 >= cols)
        {
            if (DECAWM)
            {
                move_curs(0, curs_y + 1);
            }
        }
        else
        {
            move_curs(curs_x + 1, curs_y);
        }
    }
}

void VT102::scroll(ssize_t n)
{
    curs_y += n;
    /* scroll up */
    if (n < 0)
    {
        for (ssize_t i = 0; i < -n; ++i)
        {
            for (ssize_t j = scroll_top; j < scroll_bottom; ++j)
            {
                screen.at(j) = screen.at(j + 1);
            }
            for (Char &chr : screen[scroll_bottom].chars)
            {
                chr.ch = ' ';
                chr.underline = false;
                chr.reverse = false;
                chr.blink = false;
                chr.bold = false;
                chr.charset = g[0];
            }
        }
    }
    /* scroll down */
    else if (n > 0)
    {
        for (ssize_t i = 0; i < n; ++i)
        {
            for (ssize_t j = scroll_bottom-1; j >= scroll_top; --j)
            {
                screen[j + 1] = screen[j];
            }
            for (Char &chr : screen[scroll_top].chars)
            {
                chr.ch        = ' ';
                chr.underline = false;
                chr.reverse   = false;
                chr.blink     = false;
                chr.bold      = false;
                chr.charset   = g[0];
            }
        }
    }
}

void VT102::move_curs(ssize_t x, ssize_t y)
{
    curs_x = x;
    curs_y = y;

    if (curs_x >= cols)
    {
        if (DECAWM)
        {
            curs_x = 0;
            curs_y++;
        }
        else
        {
            curs_x = cols - 1;
        }
    }
    if (curs_x < 0)
    {
        curs_x = 0;
    }
    if (curs_y > scroll_bottom)
    {
        if (DECAWM)
        {
            scroll(scroll_bottom - curs_y);
        }
        else
        {
            curs_y = scroll_bottom;
        }
    }
    if (curs_y < scroll_top)
    {
        curs_y = scroll_top;
    }
}



VT102::VT102()
:   cols(80),
    rows(24),
    curs_x(0),
    curs_y(0),
    state(State::Normal),
    keypad_mode(KPMode::Numeric),
    g{
        CharSet::UnitedStates,
        CharSet::UnitedKingdom,
        CharSet::UnitedStates,
        CharSet::UnitedKingdom },
    current_charset(0),
    single_shift(-1),
    char_attributes(0),
    KAM(false),
    IRM(false),
    SRM(false),
    LNM(false),
    DECCKM(false),
    DECANM(true),
    DECCOLM(false),
    DECSCLM(false),
    DECSCNM(false),
    DECOM(false),
    DECAWM(true),
    DECARM(false),
    DECPFF(false),
    DECPEX(false),
    online(true),
    block_cursor(true),
    margin_bell(false),
    keyclick(true),
    auto_XON_XOFF(true),
    UK_charset(false),
    break_enable(false),
    disconnect_character_enable(false),
    brightness(1.0),
    scroll_top(0),
    scroll_bottom(rows-1),
    answerback(""),
    screen(),
    cmd(nullptr),
    xon(true),
    outbuffer(""),
    saved(nullptr)
{
    for (ssize_t y = 0; y < rows; ++y)
    {
        Line line{Line::NORMAL, std::vector<Char>()};
        for (ssize_t x = 0; x < 132; ++x)
        {
            line.chars.push_back(
                (Char){' ', false, false, false, false, g[0]});
        }
        screen.push_back(line);
    }

    for (size_t x = 0; x < tab_stops.size(); ++x)
    {
        tab_stops[x] = (x != 0 && x % 8 == 0);
    }
}

VT102::VT102(const VT102 &other)
{
    if (&other != this)
    {
        cols = other.cols;
        rows = other.rows;
        curs_x = other.curs_x;
        curs_y = other.curs_y;
        state = other.state;
        keypad_mode = other.keypad_mode;
        std::copy(other.g, other.g+4, g);
        single_shift = other.single_shift;
        char_attributes = other.char_attributes;
        KAM = other.KAM;
        IRM = other.IRM;
        SRM = other.SRM;
        LNM = other.LNM;
        DECCKM = other.DECCKM;
        DECANM = other.DECANM;
        DECCOLM = other.DECCOLM;
        DECSCLM = other.DECSCLM;
        DECSCNM = other.DECSCNM;
        DECOM = other.DECOM;
        DECAWM = other.DECAWM;
        DECARM = other.DECARM;
        DECPFF = other.DECPFF;
        DECPEX = other.DECPEX;
        scroll_top = other.scroll_top;
        scroll_bottom = other.scroll_bottom;
        screen = other.screen;

        if (other.cmd != nullptr)
        {
            cmd = new ControlSequence(other.cmd);
        }
        else
        {
            cmd = nullptr;
        }
    }
}

VT102::~VT102()
{
    delete cmd;
}

