/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 * vt102.cpp
 *
 *  VT102 Emulator
 *
 */

#include "vt102.h"

#include <cstdlib>
#include <cstring>

#include <stdexcept>
#include <unordered_map>


bool VT102CONFIG_do_trace = false;


#define TRACE(fmt, ...)             \
    ({                              \
        if (VT102CONFIG_do_trace)   \
        {                           \
            fprintf(                \
                stderr,             \
                "%s: " fmt "\n",    \
                __func__,           \
                ##__VA_ARGS__);     \
        }                           \
    })



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

void VT102::keyboard_input(Key key, unsigned int mod)
{
    std::unordered_map<Key, std::array<int, 3>> const keymap =\
    {
        /* key
         *                 unshifted
         *                 |    shifted
         *                 |    |    ctrl
         *                 |    |    |
         *                 v    v    v */
        { Escape,       { 033, 033,  -1 } },

        { KB_1,         { '1', '!',  -1 } },
        { KB_2,         { '2', '@',  -1 } },
        { KB_3,         { '3', '#',  -1 } },
        { KB_4,         { '4', '$',  -1 } },
        { KB_5,         { '5', '%',  -1 } },
        { KB_6,         { '6', '^',  -1 } },
        { KB_7,         { '7', '&',  -1 } },
        { KB_8,         { '8', '*',  -1 } },
        { KB_9,         { '9', '(',  -1 } },
        { KB_0,         { '0', ')',  -1 } },
        { Minus,        { '-', '_',  -1 } },
        { Equals,       { '=', '+',  -1 } },
        { Backtick,     { '`', '~', 036 } },
        { Backspace,    {'\b','\b',  -1 } },

        { Tab,          {'\t','\t',  -1 } },
        { KB_Q,         { 'q', 'Q', 021 } },
        { KB_W,         { 'w', 'W', 027 } },
        { KB_E,         { 'e', 'E', 005 } },
        { KB_R,         { 'r', 'R', 022 } },
        { KB_T,         { 't', 'T', 024 } },
        { KB_Y,         { 'y', 'Y', 031 } },
        { KB_U,         { 'u', 'U', 025 } },
        { KB_I,         { 'i', 'I', 011 } },
        { KB_O,         { 'o', 'O', 017 } },
        { KB_P,         { 'p', 'P', 020 } },
        { LeftBracket,  { '[', '{', 033 } },
        { RightBracket, { ']', '}', 035 } },
        { Delete,       {0177,0177,  -1 } },

        { KB_A,         { 'a', 'A', 001 } },
        { KB_S,         { 's', 'S', 023 } },
        { KB_D,         { 'd', 'D', 004 } },
        { KB_F,         { 'f', 'F', 006 } },
        { KB_G,         { 'g', 'G', 007 } },
        { KB_H,         { 'h', 'H', 010 } },
        { KB_J,         { 'j', 'J', 012 } },
        { KB_K,         { 'k', 'K', 013 } },
        { KB_L,         { 'l', 'L', 014 } },
        { Semicolon,    { ';', ':',  -1 } },
        { Quote,        { '\'','"',  -1 } },
        { Backslash,    { '\\','|', 034 } },

        { KB_Z,         { 'z', 'Z', 032 } },
        { KB_X,         { 'x', 'X', 030 } },
        { KB_C,         { 'c', 'C', 003 } },
        { KB_V,         { 'v', 'V', 026 } },
        { KB_B,         { 'b', 'B', 002 } },
        { KB_N,         { 'n', 'N', 016 } },
        { KB_M,         { 'm', 'M', 015 } },
        { Comma,        { ',', '<',  -1 } },
        { Period,       { '.', '>',  -1 } },
        { Slash,        { '/', '?', 037 } },
        { LineFeed,     {'\n','\n',  -1 } },

        { Space,        { ' ', ' ', 000 } },
    };


    if (state == State::CreateAnswerback)
    {
        if (setup.delimiter == -1)
        {
            setup.delimiter = key;
        }
        else if (key == setup.delimiter)
        {
            setup.answerback_idx = 0;
            setup.delimiter = -1;
            state = State::SetUpB;
            curs_y = rows - 2;
            curs_x = 0;
        }
        else
        {
            /* TODO: translate keypresses into characters */
            char ch = 'x';
            answerback[setup.answerback_idx++] = ch;
            /* TODO: control characters appear as a diamond */
            putc(ch);

            if (setup.answerback_idx >= 20)
            {
                setup.answerback_idx = 0;
                setup.delimiter = -1;
                state = State::SetUpB;
                curs_y = rows - 2;
                curs_x = 0;
            }
        }

        display_setup();
        curs_x = 3 + (setup.delimiter != -1) + setup.answerback_idx;
        curs_y = rows - 2;
    }
    else if (state != State::SetUpA && state != State::SetUpB)
    {
        switch (key)
        {
        case SetUp:
            enter_setup();
            curs_y = rows - 2;
            curs_x = 0;
            break;

        case Up:
        case Down:
        case Left:
        case Right:
          {
            std::unordered_map<Key, char> out =\
            {
                { Up,    'A'},
                { Down,  'B'},
                { Right, 'C'},
                { Left,  'D'}
            };
            char msg[4] =\
            {
                '\033', /* ESC */
                DECCKM? 'O' : '[',
                out[key],
                '\0'
            };
            output(std::string(msg));
          } break;

        case Break:
            /* TODO: */
            break;

        case Return:
        case KP_Enter:
            if (key == KP_Enter && (mod & Ctrl))
            {
                /* TODO: toggle auto print */
                puts("auto print toggle");
            }
            else if (key == KP_Enter && (mod & Shift))
            {
                /* TODO: print screen */
                puts("print screen");
            }
            else if (key == KP_Enter && keypad_mode == KPMode::Application)
            {
                output("\0330M");
            }
            else
            {
                if (LNM)
                {
                    output("\r\n");
                }
                else
                {
                    output("\r");
                }
            }
            break;

        case NoScroll:
            /* TODO: is this right? */
            xon = !xon;
            break;

        case PF1:
        case PF2:
        case PF3:
        case PF4:
          {
            std::unordered_map<Key, char> byte3 =\
            {
                { PF1, 'P' },
                { PF2, 'Q' },
                { PF3, 'R' },
                { PF4, 'S' }
            };
            char out[4] =\
            {
                '\033', /* ESC */
                'O',
                byte3[key],
                '\0'
            };
            output(std::string(out));
          } break;

        case KP_0:
        case KP_1:
        case KP_2:
        case KP_3:
        case KP_4:
        case KP_5:
        case KP_6:
        case KP_7:
        case KP_8:
        case KP_9:
        case KP_Minus:
        case KP_Comma:
        case KP_Period:
            if (keypad_mode == KPMode::Numeric)
            {
                std::unordered_map<Key, char> out =\
                {
                    { KP_0,      '0' },
                    { KP_1,      '1' },
                    { KP_2,      '2' },
                    { KP_3,      '3' },
                    { KP_4,      '4' },
                    { KP_5,      '5' },
                    { KP_6,      '6' },
                    { KP_7,      '7' },
                    { KP_8,      '8' },
                    { KP_9,      '9' },
                    { KP_Minus,  '-' },
                    { KP_Comma,  ',' },
                    { KP_Period, '.' },
                };
                output(std::string(1, out[key]));
            }
            else
            {
                std::unordered_map<Key, char> out =\
                {
                    { KP_0,      'p' },
                    { KP_1,      'q' },
                    { KP_2,      'r' },
                    { KP_3,      's' },
                    { KP_4,      't' },
                    { KP_5,      'u' },
                    { KP_6,      'v' },
                    { KP_7,      'w' },
                    { KP_8,      'x' },
                    { KP_9,      'y' },
                    { KP_Minus,  'm' },
                    { KP_Comma,  'l' },
                    { KP_Period, 'n' },
                };
                output("\0330" + std::string(1, out[key]));
            }
            break;

        default:
          {
            int idx = 0;
            if (mod & (Shift | CapsLock))
            {
                idx = 1;
            }
            /* IMPORTANT: CTRL overrides SHIFT! */
            if (mod & Ctrl)
            {
                idx = 2;
            }
            char out = keymap.at(key).at(idx);
            if (out != -1)
            {
                output(std::string(1, out));
            }
          } break;
        }
    }
    else
    {
        /* TODO: RETURN, TAB, and SPACE move the cursor */
        switch (key)
        {
        case SetUp:
            /* exit SETUP mode */
            exit_setup();
            break;

        case KB_2:
            /* set individual tab stops */
            if (state == State::SetUpA)
            {
                setup.tab_stops[curs_x] = !setup.tab_stops[curs_x];
            }
            break;

        case KB_3:
            /* clear all tab stops */
            if (state == State::SetUpA)
            {
                for (size_t x = 0; x < setup.tab_stops.size(); ++x)
                {
                    setup.tab_stops[x] = false;
                }
            }
            /* TODO: erase the saved screen */
            break;

        case KB_4:
            /* toggle ON/OFF LINE */
            setup.online = !setup.online;
            break;

        case KB_5:
            /* toggle SETUP A/B */
            if (state == State::SetUpA)
            {
                state = State::SetUpB;
            }
            else
            {
                state = State::SetUpA;
            }
            break;

        case KB_6:
            /* toggle the selected feature */
            if (state == State::SetUpB)
            {
                switch (curs_x)
                {
                /* 1 */
                case 2:
                    DECSCLM = !DECSCLM;
                    break;
                case 3:
                    DECARM = !DECARM;
                    break;
                case 4:
                    DECSCNM = !DECSCNM;
                    break;
                case 5:
                    setup.block_cursor = !setup.block_cursor;
                    break;

                /* 2 */
                case 10:
                    setup.margin_bell = !setup.margin_bell;
                    break;
                case 11:
                    setup.keyclick = !setup.keyclick;
                    break;
                case 12:
                    DECANM = !DECANM;
                    break;
                case 13:
                    setup.auto_XON_XOFF = !setup.auto_XON_XOFF;
                    break;

                /* 3 */
                case 18:
                    setup.UK_charset = !setup.UK_charset;
                    break;
                case 19:
                    DECAWM = !DECAWM;
                    break;
                case 20:
                    LNM = !LNM;
                    break;
                case 21:
                    SRM = !SRM;
                    break;

                /* 4 */
                case 26:
                    DECPFF = !DECPFF;
                    break;
                case 27:
                    DECPEX = !DECPEX;
                    break;
                case 28:
                    setup.stop_bits = !setup.stop_bits;
                    break;
                case 29:
                    setup.receive_parity = !setup.receive_parity;
                    break;

                /* 5 */
                case 34:
                    setup.break_enable = !setup.break_enable;
                    break;
                case 35:
                    setup.disconn_char_enable =\
                        !setup.disconn_char_enable;
                    break;
                case 36:
                    setup.disconn_delay = !setup.disconn_delay;
                    break;
                case 37:
                    setup.auto_answerback = !setup.auto_answerback;
                    break;

                /* 6 */
                case 42:
                    setup.initial_direction =\
                        !setup.initial_direction;
                    break;
                case 43:
                    setup.auto_turnaround = !setup.auto_turnaround;
                    break;

                /* 7 */
                case 50:
                    setup.power = !setup.power;
                    break;
                case 51:
                    setup.wps_terminal_kbd = !setup.wps_terminal_kbd;
                    break;

                default:
                    /* no feature selected */
                    break;
                }
            }
            break;

        case KB_7:
            if (state == State::SetUpB)
            {
                /* set tx speed for modem */
                if (modem_features_selected)
                {
                    setup.modem.tx_speed++;
                    setup.modem.tx_speed %= 16;
                }
                /* set tx speed for printer */
                else
                {
                    setup.printer.tx_rx_speed++;
                    setup.printer.tx_rx_speed %= 16;
                }
            }
            break;

        case KB_8:
            if (state == State::SetUpB)
            {
                /* set rx speed for modem */
                if (modem_features_selected)
                {
                    setup.modem.rx_speed++;
                    setup.modem.rx_speed %= 16;
                }
                /* set rx speed for printer */
                else
                {
                    setup.printer.tx_rx_speed++;
                    setup.printer.tx_rx_speed %= 16;
                }
            }
            break;

        case KB_9:
            /* toggle 80/132 column mode */
            DECCOLM = !DECCOLM;
            break;

        case KB_0:
            /* TODO: reset */
            break;

        case Up:
            /* increase brightness */
            if (setup.brightness + 0.1 <= 1.0)
            {
                setup.brightness += 0.1;
            }
            break;

        case Down:
            /* decrease brightness */
            if (setup.brightness - 0.1 > 0)
            {
                setup.brightness -= 0.1;
            }
            break;

        case Left:
            /* select modem features */
            if (state == State::SetUpB && (mod & Shift))
            {
                modem_features_selected = true;
            }
            /* move cursor left */
            else
            {
                if (curs_x - 1 >= 0)
                {
                    curs_x--;
                }
            }
            break;

        case Right:
            /* select printer features */
            if (state == State::SetUpB && (mod & Shift))
            {
                modem_features_selected = false;
            }
            /* move cursor right */
            else
            {
                if (curs_x + 1 < cols)
                {
                    curs_x++;
                }
            }
            break;

        case KB_A:
            /* create the answerback message */
            if (state == State::SetUpB && (mod & Shift))
            {
                state = State::CreateAnswerback;
            }
            break;

        case KB_C:
            /* select turnaround/disconnect characters */
            if (state == State::SetUpB && (mod & Shift))
            {
                setup.modem.turnaround_disconn_char++;
                setup.modem.turnaround_disconn_char %= 5;
            }
            break;

        case KB_D:
            /* reset features to default */
            if (mod & Shift)
            {
                setup_defaults();
            }
            break;

        case KB_M:
            /* select modem control */
            if (state == State::SetUpB && (mod & Shift))
            {
                setup.modem.control++;
                setup.modem.control %= 5;
            }
            break;

        case KB_P:
            /*  select data/parity bits for modem or printer */
            if (state == State::SetUpB && (mod & Shift))
            {
                /* select modem data/parity bits */
                if (modem_features_selected)
                {
                    setup.modem.data_parity_bits++;
                    setup.modem.data_parity_bits %= 8;
                }
                else
                {
                    setup.printer.data_parity_bits++;
                    setup.printer.data_parity_bits %= 8;
                }
            }
            break;

        case KB_R:
            /* load user_setup into setup */
            if (mod & Shift)
            {
                setup = user_setup;
            }
            break;

        case KB_S:
            /* save setup into user_setup */
            if (mod & Shift)
            {
                user_setup = setup;
            }
            break;

        case KB_T:
            /* reset tab stops to default */
            for (size_t x = 0; x < setup.tab_stops.size(); ++x)
            {
                setup.tab_stops[x] = (x != 0 && x % 8 == 0);
            }
            break;


        default:
            /* other keys are ignored */
            break;
        }
        if (key != SetUp)
        {
            ssize_t x = curs_x,
                    y = curs_y;
            display_setup();
            curs_x = (state == State::CreateAnswerback? 3 : x);
            curs_y = y;
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
            TRACE("DECDHL upper");
            screen[curs_y].attr = Line::DOUBLE_HEIGHT_UPPER;
            break;

        /* DECDHL: lower half double-height double-width */
        case '4':
            TRACE("DECDHL lower");
            screen[curs_y].attr = Line::DOUBLE_HEIGHT_LOWER;
            break;

        /* DECSWL: single-height single-width */
        case '5':
            TRACE("DECSWL");
            screen[curs_y].attr = Line::NORMAL;
            break;

        /* DECDWL: single-height double-width */
        case '6':
            TRACE("DECDWL");
            screen[curs_y].attr = Line::DOUBLE_WIDTH;
            break;

        /* DECALN */
        case '8':
            TRACE("DECALN");
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
        TRACE("G0 select '%c'", ch);
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
                + "`");
            break;
        }
        state = State::Normal;
        break;

    case State::G1SetSelect:
        TRACE("G1 select '%c'", ch);
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

    case State::SetUpA:
    case State::SetUpB:
    case State::CreateAnswerback:
        /* in SETUP mode, incoming computer characters are ignored */
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
        TRACE("NUL");
        break;

    /* ETX */
    case '\003':
        /* TODO: selectable as half-duplex turnaround */
        TRACE("ETX");
        throw std::runtime_error("ETX not implemented");
        break;

    /* EOT */
    case '\004':
        /* TODO: selectable as half-duplex turnaround or disconnect */
        TRACE("EOT");
        throw std::runtime_error("EOT not implemented");
        break;

    /* ENQ */
    case '\005':
        TRACE("ENQ");
        output(std::string(answerback));
        break;

    /* BEL */
    case '\a':
        /* TODO: beep */
        TRACE("BEL");
        puts("boop");
        break;

    /* BS */
    case '\b':
        TRACE("BS");
        if (curs_x - 1 >= 0)
        {
            curs_x -= 1;
        }
        break;

    /* HT */
    case '\t':
      {
        TRACE("HT");
        ssize_t tmp = curs_x;
        /* HT moves the cursor to the next tab stop,
         * or to the right margin if there are no more tab stops */
        curs_x = cols - 1;
        for (ssize_t x = tmp + 1; x < cols; ++x)
        {
            if (setup.tab_stops[x])
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
        TRACE("%s",
            (ch == '\n' || ch == '\v')
                ? (ch == '\n')? "LF" : "VT"
                : "FF");
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
        TRACE("CR");
        curs_x = 0;
        /* TODO: can be selected as half-duplex turnaround */
        break;

    /* SO */
    case '\016':
        TRACE("SO");
        current_charset = 1;
        break;

    /* SI */
    case '\017':
        TRACE("SI");
        current_charset = 0;
        break;

    /* DC1 */
    case '\021':
        TRACE("DC1");
        if (setup.auto_XON_XOFF)
        {
            xon = true;
        }
        break;

    /* DC3 */
    case '\023':
        /* TODO: can be selected as half-duplex turnaround */
        TRACE("DC3");
        if (setup.auto_XON_XOFF)
        {
            xon = false;
        }
        break;

    /* CAN, SUB */
    case '\030':
    case '\032':
        TRACE("%s", ch == '\030'? "CAN" : "SUB");
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
        TRACE("ESC");
        state = State::Escape;
        break;

    /* DEL */
    case '\177':
        /* ignored */
        TRACE("DEL");
        break;


    /* normal character */
    default:
        TRACE("%c", ch);
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
        TRACE("RIS");
        throw std::runtime_error("RIS not implemented");
        break;

    /* IND */
    case 'D':
        TRACE("IND");
        curs_y += 1;
        if (curs_y > scroll_bottom)
        {
            scroll(-1);
        }
        break;

    /* NEL */
    case 'E':
        TRACE("NEL");
        curs_x = 0;
        curs_y += 1;
        if (curs_y > scroll_bottom)
        {
            scroll(-1);
        }
        break;

    /* HTS */
    case 'H':
        TRACE("HTS");
        setup.tab_stops[curs_x] = true;
        break;

    /* RI */
    case 'M':
        TRACE("RI");
        curs_y -= 1;
        if (curs_y < scroll_top)
        {
            scroll(+1);
        }
        break;

    /* SS2 */
    case 'N':
        TRACE("SS2");
        single_shift = 2;
        break;

    /* DECID */
    case 'Z':
        TRACE("DECID");
        output("\033[?6c");
        break;

    /* SS3 */
    case '0':
        TRACE("SS3");
        single_shift = 3;
        break;

    /* DECSC */
    case '7':
        TRACE("DECSC");
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
        TRACE("DECRC");
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
        TRACE("CSI");
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
                TRACE("CUU %d", delta);
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
                TRACE("CUD %d", delta);
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
                TRACE("CUF %d", delta);
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
                TRACE("CUB %d", delta);
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
                TRACE("%s %d %d",
                    ch == 'H'? "CUP" : "HVP",
                    newx,
                    newy);
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
                    throw std::runtime_error(
                        "ED takes up to 1 parameter");
                    break;
                }
                switch (arg)
                {
                case 0:
                    /* erase from cursor to end of screen */
                    TRACE("ED curs to end of screen");
                    for (ssize_t y = curs_y; y < rows; ++y)
                    {
                        for (
                            ssize_t x = (y == curs_y? curs_x : 0);
                            x < cols;
                            ++x)
                        {
                            erase(x, y);
                        }
                        screen[y].attr = Line::NORMAL;
                    }
                    break;
                case 1:
                    /* erase from start of screen to cursor */
                    TRACE("ED start of screen to curs");
                    for (ssize_t y = 0; y <= curs_y; ++y)
                    {
                        for (
                            ssize_t x = 0;
                            x <= (y == curs_y? curs_x : cols);
                            ++x)
                        {
                            erase(x, y);
                        }
                        screen[y].attr = Line::NORMAL;
                    }
                    break;
                case 2:
                    /* erase entire display */
                    TRACE("ED entire display");
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
                    throw std::runtime_error(
                        "ED only accepts 1,2, or 3 as a parameter");
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
                    throw std::runtime_error(
                        "EL takes up to 1 parameter");
                    break;
                }
                switch (arg)
                {
                case 0:
                    /* erase from cursor to end of line */
                    TRACE("EL curs to end of line");
                    for (ssize_t i = curs_x; i < cols; ++i)
                    {
                        erase(i, curs_y);
                    }
                    break;
                case 1:
                    /* erase from start of line to cursor */
                    TRACE("EL start of line to cursor");
                    for (ssize_t i = 0; i <= curs_x; ++i)
                    {
                        erase(i, curs_y);
                    }
                    break;
                case 2:
                    /* erase entire line */
                    TRACE("EL entire line");
                    for (ssize_t i = 0; i < cols; ++i)
                    {
                        erase(i, curs_y);
                    }
                    break;

                default:
                    throw std::runtime_error(
                        "EL only accepts 0, 1 or 2 as a parameter");
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
                TRACE("IL %d", arg);
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
                TRACE("DL %d", arg);
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
                TRACE("DCH %d", arg);
                for (int i = 0; i < arg; ++i)
                {
                    del_char(curs_x, curs_y);
                }
              } break;

            /* DA */
            case 'c':
                TRACE("DA");
                output("\033[?6c");
                break;

            /* TBC */
            case 'g':
                switch (cmd->params.size())
                {
                /* clear tab stop at current position */
                case 0:
                    TRACE("TBC current position");
                    setup.tab_stops[curs_x] = false;
                    break;
                case 1:
                    if (cmd->params[0] == "0")
                    {
                        /* clear tab stop at current position */
                        TRACE("TBC current position");
                        setup.tab_stops[curs_x] = false;
                    }
                    else if (cmd->params[0] == "3")
                    {
                        /* clear all tab stops */
                        TRACE("TBC all");
                        for (
                            size_t x = 0;
                            x < setup.tab_stops.size();
                            ++x)
                        {
                            setup.tab_stops[x] = false;
                        }
                        break;
                    }
                    else
                    {
                        /* TBC ignores undefined parameters */
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
                        TRACE("%cM KAM", ch == 'h'? 'S' : 'R');
                        KAM = setting;
                        break;
                    case 4:
                        TRACE("%cM IRM", ch == 'h'? 'S' : 'R');
                        IRM = setting;
                        break;
                    case 12:
                        TRACE("%cM SRM", ch == 'h'? 'S' : 'R');
                        SRM = setting;
                        break;
                    case 20:
                        TRACE("%cM LNM", ch == 'h'? 'S' : 'R');
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
                            TRACE("%cM DECCKM", ch == 'h'? 'S' : 'R');
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
                            TRACE("%cM DECANM", ch == 'h'? 'S' : 'R');
                            if (!setting)
                            {
                                DECANM = true;
                                throw std::runtime_error(
                                    "VT52 compatibility mode"
                                    "unimplemented!");
                            }
                            break;
                        case 3:
                            TRACE("%cM DECCOLM",
                                ch == 'h'? 'S' : 'R');
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
                            TRACE("%cM DECSCLM",
                                ch == 'h'? 'S' : 'R');
                            DECSCLM = setting;
                            break;
                        case 5:
                            TRACE("%cM DECSCNM",
                                ch == 'h'? 'S' : 'R');
                            DECSCNM = setting;
                            break;
                        case 6:
                            TRACE("%cM DECOM",
                                ch == 'h'? 'S' : 'R');
                            DECOM = setting;
                            /* the cursor moves to the new home
                             * position when DECOM is changed */
                            move_curs(0, DECOM? scroll_top : 0);
                            break;
                        case 7:
                            TRACE("%cM DECAWM", ch == 'h'? 'S' : 'R');
                            DECAWM = setting;
                            break;
                        case 8:
                            TRACE("%cM DECARM", ch == 'h'? 'S' : 'R');
                            DECARM = setting;
                            break;
                        case 18:
                            TRACE("%cM DECPFF", ch == 'h'? 'S' : 'R');
                            DECPFF = setting;
                            break;
                        case 19:
                            TRACE("%cM DECPEX", ch == 'h'? 'S' : 'R');
                            DECPEX = setting;
                            break;

                        default:
                            throw std::runtime_error(
                                std::string(setting? "S" : "R")
                                + "M - undefined DEC Private "
                                + "Mode sequence"
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
                TRACE("MC");
                break;

            /* SGR */
            case 'm':
                if (cmd->params.size() == 0)
                {
                    TRACE("SGR off");
                    char_attributes = 0;
                }
                else
                {
                    std::string debug_string = "SGR";
                    for (std::string param : cmd->params)
                    {
                        int attr = (param == "")
                            ? 0
                            : std::stoi(param);
                        switch (attr)
                        {
                        case 0:
                            debug_string += " off";
                            char_attributes = 0;
                            break;
                        case 1:
                            debug_string += " bold";
                            char_attributes |= BOLD;
                            break;
                        case 4:
                            debug_string += " underline";
                            char_attributes |= UNDERLINE;
                            break;
                        case 5:
                            debug_string += " blink";
                            char_attributes |= BLINK;
                            break;
                        case 7:
                            debug_string += " reverse";
                            char_attributes |= REVERSE;
                            break;

                        default:
                            throw std::runtime_error(
                                std::string(
                                    "SGR - undefined DEC Private "
                                    "Mode sequence")
                                + cmd->params[1]);
                            break;
                        }
                    }
                    TRACE("%s", debug_string.c_str());
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
                                /*  `ESC [ ? 13 n` - no printer
                                 *                   connected
                                 *  `ESC [ ? 11 n` - printer not ready
                                 *  `ESC [ ? 10 n` - printer ready */
                                TRACE("DSR printer status");
                                output("\033[?13n");
                            }
                        }
                        else
                        {
                            throw std::runtime_error(
                                std::string(
                                    "undefined DEC Private Mode "
                                    "sequence `ESC [ ")
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
                            TRACE("DSR status");
                            output("\033[0n");
                            break;
                        case 6:
                            /* `ESC [ curs_y ; curs_x R` */
                            TRACE("DSR cursor position");
                            output(
                                "\033["
                                + std::to_string(
                                    scroll_top + curs_y + 1)
                                + ";"
                                + std::to_string(curs_x + 1)
                                + "R");
                            break;
                        }
                    }
                    else
                    {
                        throw std::runtime_error(
                            "DSR takes 1 or 2 arguments");
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
                        TRACE("DECLL on");
                    }
                    /* LED off */
                    else if (cmd->params[0] == "1")
                    {
                        TRACE("DECLL off");
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
                TRACE("DECSTBM %d %d", top, bottom);
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
                TRACE("DECTST");
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

void VT102::enter_setup(void)
{
    saved_screen = screen;
    saved_state = state;

    state = State::SetUpA;

    display_setup();
}

void VT102::display_setup(void)
{
    /* clear the screen */
    for (ssize_t y = 0; y < rows; ++y)
    {
        for (ssize_t x = 0; x < cols; ++x)
        {
            erase(x, y);
        }
        screen[curs_y].attr = Line::NORMAL;
    }

    curs_x = 0;
    curs_y = 0;

    screen[curs_y].attr = Line::DOUBLE_HEIGHT_UPPER;
    char_attributes = BOLD;
    for (char ch : state == State::SetUpA? "SET-UP A" : "SET-UP B")
    {
        putc(ch);
    }
    curs_x = 0;
    screen[++curs_y].attr = Line::DOUBLE_HEIGHT_LOWER;
    for (char ch : state == State::SetUpA? "SET-UP A" : "SET-UP B")
    {
        putc(ch);
    }

    curs_x = 0;
    screen[++curs_y].attr = Line::DOUBLE_WIDTH;
    char_attributes = UNDERLINE;
    for (char ch : "TO EXIT PRESS \"SET-UP\"")
    {
        if (ch != 0)
        {
            putc(ch);
        }
    }
    char_attributes = 0;

    if (state == State::SetUpA)
    {
        curs_y = rows - 1;
        curs_x = 0;
        bool inverted = false;
        for (ssize_t x = 0; x < cols; ++x)
        {
            inverted = ((x / 10) % 2 == 1);
            char ch = '0' + ((x + 1) % 10);
            putc(ch);
            screen[rows - 1][x].reverse = inverted;
        }

        curs_y = rows - 2;
        curs_x = 0;
        for (ssize_t x = 0; x < cols; ++x)
        {
            if  (setup.tab_stops[x])
            {
                putc('T');
            }
            else
            {
                putc(' ');
            }
        }
    }
    else
    {
        unsigned tmp = char_attributes;

        curs_y = rows - 6;
        curs_x = 1;

        for (char ch : "V1.1")
        {
            putc(ch);
        }
        for (size_t i = 0; i < 10; ++i)
        {
            putc(' ');
        }
        char_attributes = BOLD | UNDERLINE;
        putc('M');
        putc('O');
        putc('D');
        putc('E');
        putc('M');
        char_attributes = 0;
        for (size_t i = 0; i < 30; ++i)
        {
            putc(' ');
        }
        char_attributes = BOLD | UNDERLINE;
        putc('P');
        putc('R');
        putc('I');
        putc('N');
        putc('T');
        putc('E');
        putc('R');
        char_attributes = 0;


        curs_y = rows - 4;
        curs_x = 2;

        char_attributes = modem_features_selected? REVERSE : 0;

        /* modem data/parity bits */
        std::string const parities[8] =\
        {
            "7M",
            "7S",
            "7O",
            "7E",
            "7N",
            "8O",
            "8E",
            "8N",
        };
        putc('P');
        putc('=');
        putc(parities[setup.modem.data_parity_bits][0]);
        putc(parities[setup.modem.data_parity_bits][1]);

        putc(' ');
        putc(' ');

        /* modem tx speed */
        std::string const tx_rx_speeds[16] =\
        {
            "   50",  "   75",  "  110", "134.5",
            "  150",  "  200",  "  300", "  600",
            " 1200",  " 1800",  " 2000", " 2400",
            " 3600",  " 4800",  " 9600", "19200",
        };
        putc('T');
        putc('=');
        putc(tx_rx_speeds[setup.modem.tx_speed][0]);
        putc(tx_rx_speeds[setup.modem.tx_speed][1]);
        putc(tx_rx_speeds[setup.modem.tx_speed][2]);
        putc(tx_rx_speeds[setup.modem.tx_speed][3]);
        putc(tx_rx_speeds[setup.modem.tx_speed][4]);

        putc(' ');
        putc(' ');

        /* modem rx speed */
        putc('R');
        putc('=');
        putc(tx_rx_speeds[setup.modem.rx_speed][0]);
        putc(tx_rx_speeds[setup.modem.rx_speed][1]);
        putc(tx_rx_speeds[setup.modem.rx_speed][2]);
        putc(tx_rx_speeds[setup.modem.rx_speed][3]);
        putc(tx_rx_speeds[setup.modem.rx_speed][4]);

        putc(' ');
        putc(' ');

        /* modem_control */
        std::string const control[5] =\
        {
            "FDX A",
            "FDX B",
            "FDX C",
            "HDX A",
            "HDX B",
        };
        putc(control[setup.modem.control][0]);
        putc(control[setup.modem.control][1]);
        putc(control[setup.modem.control][2]);
        putc(control[setup.modem.control][3]);
        putc(control[setup.modem.control][4]);

        putc(' ');
        putc(' ');

        /* turnaround/disconnect character */
        std::string const turnaround[6][2] =\
        {
            { "   ", "   " },
            { "FF ", "EOT" },
            { "ETX", "EOT" },
            { "EOT", "DLE" },
            { "CR ", "EOT" },
            { "DC3", "EOT" },
        };
        std::string const *characters =\
            turnaround[setup.modem.turnaround_disconn_char];
        putc(characters[setup.modem.control != 4][0]);
        putc(characters[setup.modem.control != 4][1]);
        putc(characters[setup.modem.control != 4][2]);


        char_attributes = 0;
        for (size_t i = 0; i < 11; ++i)
        {
            putc(' ');
        }


        char_attributes = modem_features_selected? 0 : REVERSE;

        /* printer data/parity bits */
        putc('P');
        putc('=');
        putc(parities[setup.printer.data_parity_bits][0]);
        putc(parities[setup.printer.data_parity_bits][1]);

        putc(' ');
        putc(' ');

        /* printer tx/rx speed */
        putc('T');
        putc('/');
        putc('R');
        putc('=');
        putc(tx_rx_speeds[setup.printer.tx_rx_speed][0]);
        putc(tx_rx_speeds[setup.printer.tx_rx_speed][1]);
        putc(tx_rx_speeds[setup.printer.tx_rx_speed][2]);
        putc(tx_rx_speeds[setup.printer.tx_rx_speed][3]);
        putc(tx_rx_speeds[setup.printer.tx_rx_speed][4]);


        curs_y = rows - 1;
        curs_x = 0;

        char_attributes = 0;
        putc('1');
        putc(' ');
        char_attributes |= REVERSE;
        putc(DECSCLM? '1' : '0');
        putc(DECARM? '1' : '0');
        putc(DECSCNM? '1' : '0');
        putc(setup.block_cursor? '1' : '0');

        char_attributes = 0;
        putc(' ');
        putc(' ');

        putc('2');
        putc(' ');
        char_attributes |= REVERSE;
        putc(setup.margin_bell? '1' : '0');
        putc(setup.keyclick? '1' : '0');
        putc(DECANM? '1' : '0');
        putc(setup.auto_XON_XOFF? '1' : '0');

        char_attributes = 0;
        putc(' ');
        putc(' ');

        putc('3');
        putc(' ');
        char_attributes |= REVERSE;
        putc(setup.UK_charset? '1' : '0');
        putc(DECAWM? '1' : '0');
        putc(LNM? '1' : '0');
        putc(SRM? '1' : '0');

        char_attributes = 0;
        putc(' ');
        putc(' ');

        putc('4');
        putc(' ');
        char_attributes |= REVERSE;
        putc(DECPFF? '1' : '0');
        putc(DECPEX? '1' : '0');
        putc(setup.stop_bits? '1' : '0');
        putc(setup.receive_parity? '1' : '0');

        char_attributes = 0;
        putc(' ');
        putc(' ');

        putc('5');
        putc(' ');
        char_attributes |= REVERSE;
        putc(setup.break_enable? '1' : '0');
        putc(setup.disconn_char_enable? '1' : '0');
        putc(setup.disconn_delay? '1' : '0');
        putc(setup.auto_answerback? '1' : '0');

        char_attributes = 0;
        putc(' ');
        putc(' ');

        putc('6');
        putc(' ');
        char_attributes |= REVERSE;
        putc(setup.initial_direction? '1' : '0');
        putc(setup.auto_turnaround? '1' : '0');
        putc('0');  /* always 0 */
        putc('0');  /* always 0 */

        char_attributes = 0;
        putc(' ');
        putc(' ');

        putc('7');
        putc(' ');
        char_attributes |= REVERSE;
        putc(setup.power? '1' : '0');
        putc(setup.wps_terminal_kbd? '1' : '0');
        putc('1');  /* always 1 */
        putc('0');  /* always 0 */

        char_attributes = tmp;
    }
    if (state == State::CreateAnswerback)
    {
        unsigned int tmp = char_attributes;

        curs_y = rows - 2;
        curs_x = 0;
        char_attributes = BOLD;
        putc('A');
        putc('=');
        char_attributes = 0;
        putc(' ');
        putc(setup.delimiter);
        for (char ch : answerback)
        {
            putc(ch);
        }
        char_attributes = REVERSE;
        putc(setup.delimiter);
        char_attributes = tmp;
    }
}

void VT102::setup_defaults(void)
{
    /* general */
    setup.online = true;
    setup.block_cursor = true;
    setup.margin_bell = false;
    setup.keyclick = true;
    setup.auto_XON_XOFF = true;
    setup.UK_charset = false;
    setup.stop_bits = false;
    setup.receive_parity = true;
    setup.break_enable = true;
    setup.disconn_char_enable = false;
    setup.disconn_delay = true;
    setup.auto_answerback = false;
    setup.initial_direction = true;
    setup.auto_turnaround = false;
    setup.power = true;
    setup.wps_terminal_kbd = false;
    setup.delimiter = -1;
    setup.answerback_idx = 0;
    setup.brightness = 1.0;
    setup.tab_stops = std::array<bool, 132>();
    for (size_t x = 0; x < setup.tab_stops.size(); ++x)
    {
        setup.tab_stops[x] = (x != 0 && x % 8 == 0);
    }

    /* modem */
    setup.modem.data_parity_bits = 1;
    setup.modem.tx_speed = 14;
    setup.modem.rx_speed = 14;
    setup.modem.control = 0;
    setup.modem.turnaround_disconn_char = 0;

    /* printer */
    setup.printer.data_parity_bits = 1;
    setup.printer.tx_rx_speed = 6;
}

void VT102::exit_setup(void)
{
    state = saved_state;
    screen = saved_screen;
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

size_t VT102::fontidx(CharSet charset, unsigned char ch) const
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
    if (DECAWM && curs_y > scroll_bottom)
    {
        scroll(scroll_bottom - curs_y);
    }

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
                curs_x = 0;
                curs_y++;
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
    DECSCLM(true),
    DECSCNM(false),
    DECOM(false),
    DECAWM(true),
    DECARM(true),
    DECPFF(true),
    DECPEX(true),
    user_setup({}),
    modem_features_selected(true),
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

    setup_defaults();
}

VT102::~VT102()
{
    delete cmd;
}

