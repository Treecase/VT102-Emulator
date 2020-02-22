/* Copyright (C) 2020 Trevor Last
 * See LICENSE file for copyright and license details.
 * vt102.h
 *
 *  VT102 Emulator
 *
 */

#ifndef _VT102_H
#define _VT102_H

#include <string>
#include <vector>
#include <array>

extern bool VT102CONFIG_do_trace;


struct ControlSequence
{
    std::vector<std::string> params;
    std::string intermediate;
    uint8_t final;

    ControlSequence()
    {
        final = 0;
    }
    ControlSequence(ControlSequence *const &other)
    {
        params = other->params;
        intermediate = other->intermediate;
        final = other->final;
    }
};


enum class CharSet
{
    UnitedStates,
    UnitedKingdom,
    Special,
    AltROM,
    AltROMSpecial
};


struct Char
{
    unsigned char ch;
    bool underline,
         reverse,
         blink,
         bold;
    CharSet charset;
};


struct Line
{
    enum
    {
        NORMAL,
        DOUBLE_HEIGHT_UPPER,
        DOUBLE_HEIGHT_LOWER,
        DOUBLE_WIDTH,
    } attr;

    std::vector<Char> chars;

    Char &operator[](size_t idx)
    {
        return chars[idx];
    }
    Char const &operator[](size_t idx) const
    {
        return chars[idx];
    }
};


class VT102
{
public:
    enum Key
    {
        SetUp,
        Up,
        Down,
        Left,
        Right,

        Escape,
        KB_1,
        KB_2,
        KB_3,
        KB_4,
        KB_5,
        KB_6,
        KB_7,
        KB_8,
        KB_9,
        KB_0,
        Minus,
        Equals,
        Backtick,
        Backspace,
        Break,

        Tab,
        KB_Q,
        KB_W,
        KB_E,
        KB_R,
        KB_T,
        KB_Y,
        KB_U,
        KB_I,
        KB_O,
        KB_P,
        LeftBracket,
        RightBracket,
        Return,
        Delete,

        KB_A,
        KB_S,
        KB_D,
        KB_F,
        KB_G,
        KB_H,
        KB_J,
        KB_K,
        KB_L,
        Semicolon,
        Quote,
        Backslash,

        NoScroll,
        KB_Z,
        KB_X,
        KB_C,
        KB_V,
        KB_B,
        KB_N,
        KB_M,
        Comma,
        Period,
        Slash,
        LineFeed,

        Space,

        PF1,
        PF2,
        PF3,
        PF4,

        KP_7,
        KP_8,
        KP_9,
        KP_Minus,

        KP_4,
        KP_5,
        KP_6,
        KP_Comma,

        KP_1,
        KP_2,
        KP_3,
        KP_Enter,

        KP_0,
        KP_Period,
    };
    enum Modifiers
    {
        None     = 0,
        Ctrl     = 1 << 0,
        Shift    = 1 << 1,
        CapsLock = 1 << 2
    };

    ssize_t cols,
            rows;

    ssize_t curs_x,
            curs_y;

    enum class State
    {
        Normal,
        Escape,
        CtrlSeq,
        Pound,
        G0SetSelect,
        G1SetSelect,
        SetUpA,
        SetUpB,
        CreateAnswerback,
    };

    State state,
          saved_state;

    enum class KPMode
    {
        Numeric,
        Application
    } keypad_mode;

    CharSet g[4];
    int current_charset;
    int single_shift;

    enum
    {
        BOLD      = 1 << 0,
        UNDERLINE = 1 << 1,
        BLINK     = 1 << 2,
        REVERSE   = 1 << 3
    };
    unsigned char_attributes;

    bool KAM,       /* keyboard locked/unlocked */
         IRM,       /* replacement/insertion mode */
         SRM,       /* send-receive off/on */
         LNM;       /* linefeed/newline off/on */
    bool DECCKM,    /* cursor/application */
         DECANM,    /* VT52/ANSI mode */
         DECCOLM,   /* 80/132 columns */
         DECSCLM,   /* jump/smooth scrolling */
         DECSCNM,   /* normal/inverse colours */
         DECOM,     /* absolute/relative origin */
         DECAWM,    /* autowrap off/on */
         DECARM,    /* auto-repeat off/on */
         DECPFF,    /* print form feed off/on */
         DECPEX;    /* print full screen/scroll region */

    struct
    {
        bool online,
             block_cursor,
             margin_bell,
             keyclick,
             auto_XON_XOFF,
             UK_charset,
             stop_bits,
             receive_parity,
             break_enable,
             disconn_char_enable,
             disconn_delay,
             auto_answerback,
             initial_direction,
             auto_turnaround,
             power,
             wps_terminal_kbd;
        int delimiter,
            answerback_idx;
        double brightness;
        std::array<bool, 132> tab_stops;

        struct
        {
            int data_parity_bits,
                tx_speed,
                rx_speed,
                control,
                turnaround_disconn_char;
        } modem;
        struct
        {
            int data_parity_bits,
                tx_rx_speed;
        } printer;
    } setup,
      user_setup;
    bool modem_features_selected;

    int scroll_top,
        scroll_bottom;

    char answerback[20];

    std::vector<Line> screen,
                      saved_screen;

    ControlSequence *cmd;

    bool xon;
    std::string outbuffer;


    struct SavedData
    {
        ssize_t x,
                y;
        unsigned charattr;
        int charset;
        bool DECOM;
    } *saved;


    void output(std::string message);
    void keyboard_input(Key key, unsigned int mod);

    void interpret_byte(uint8_t ch);
    void interpret_byte_control_character(uint8_t ch);
    void interpret_byte_escape(uint8_t ch);
    void interpret_byte_ctrlseq(uint8_t ch);

    void enter_setup(void);
    void display_setup(void);
    void setup_defaults(void);
    void exit_setup(void);


    /* get the character at the given x,y coords */
    Char getc_at(ssize_t x, ssize_t y) const;

    /* get the font index of ch in the given charset */
    size_t fontidx(CharSet charset, unsigned char ch) const;

    /* erase the character at the given position */
    void erase(ssize_t x, ssize_t y);

    /* delete the character at the given position
     * (moves everything on the given line left by one position) */
    void del_char(ssize_t x, ssize_t y);

    /* insert a blank character at the given position
     * (moves everything on the given line right by one position) */
    void ins_char(ssize_t x, ssize_t y);

    /* delete given line */
    void del_line(ssize_t y);

    /* insert a new blank line at the given position */
    void ins_line(ssize_t y);

    /* write a character at the cursor position */
    void putc(unsigned char ch);

    /* scroll the screen up/down by n lines */
    void scroll(ssize_t n);

    /* move the cursor to the given position */
    void move_curs(ssize_t x, ssize_t y);

    VT102();
    VT102(const VT102 &other);
    ~VT102();
};



#endif

