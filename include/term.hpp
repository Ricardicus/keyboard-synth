// term.hpp
#pragma once

#include <cstdarg>
#include <cstdio>

#ifdef BUILD_WITH_NCURSES
#include <ncurses.h>
#endif

namespace term {

enum class Style {
  Plain,
  White,     // regular white
  WhiteBold, // white + bold
  Yellow,
  GreenBold,
  BlueBold,
  Gray,   // dim white (or gray-ish)
  Purple, // magenta/purple
  Key     // key caps (black on white), like your old COLOR_PAIR(6)
};

namespace detail {

inline void init_if_needed() {
  static bool initialized = false;
  if (initialized)
    return;

#ifdef BUILD_WITH_NCURSES
  // Enable colors only once
  start_color();

  // Pair IDs (keep them stable)
  // 2: green on black
  // 4: white on black
  // 5: yellow on black
  // 6: black on white (keys)
  // 7: blue on white
  // 8: purple (magenta) on black
  init_pair(2, COLOR_GREEN, COLOR_BLACK);
  init_pair(4, COLOR_WHITE, COLOR_BLACK);
  init_pair(5, COLOR_YELLOW, COLOR_BLACK);
  init_pair(6, COLOR_BLACK, COLOR_WHITE);
  init_pair(7, COLOR_BLUE, COLOR_WHITE);
  init_pair(8, COLOR_MAGENTA, COLOR_BLACK);
#endif

  initialized = true;
}

inline void begin_style(Style s) {
  init_if_needed();

#ifdef BUILD_WITH_NCURSES
  switch (s) {
  case Style::Plain:
    break;

  case Style::White:
    attron(COLOR_PAIR(4));
    break;

  case Style::WhiteBold:
    attron(COLOR_PAIR(4) | A_BOLD);
    break;

  case Style::Yellow:
    attron(COLOR_PAIR(5));
    break;

  case Style::GreenBold:
    attron(COLOR_PAIR(2) | A_BOLD);
    break;

  case Style::BlueBold:
    attron(COLOR_PAIR(7) | A_BOLD);
    break;

  case Style::Gray:
    // ncurses doesn't always have "gray"; dim white is a good approximation.
    attron(COLOR_PAIR(4) | A_DIM);
    break;

  case Style::Purple:
    attron(COLOR_PAIR(8));
    break;

  case Style::Key:
    attron(COLOR_PAIR(6));
    break;
  }
#else
  (void)s;
#endif
}

inline void end_style(Style s) {
#ifdef BUILD_WITH_NCURSES
  switch (s) {
  case Style::Plain:
    break;

  case Style::White:
    attroff(COLOR_PAIR(4));
    break;

  case Style::WhiteBold:
    attroff(COLOR_PAIR(4) | A_BOLD);
    break;

  case Style::Yellow:
    attroff(COLOR_PAIR(5));
    break;

  case Style::GreenBold:
    attroff(COLOR_PAIR(2) | A_BOLD);
    break;

  case Style::BlueBold:
    attroff(COLOR_PAIR(7) | A_BOLD);
    break;

  case Style::Gray:
    attroff(COLOR_PAIR(4) | A_DIM);
    break;

  case Style::Purple:
    attroff(COLOR_PAIR(8));
    break;

  case Style::Key:
    attroff(COLOR_PAIR(6));
    break;
  }
#else
  (void)s;
#endif
}

inline void vprint_impl(const char *fmt, va_list args) {
#ifdef BUILD_WITH_NCURSES
  // Use the default screen
  vw_printw(stdscr, fmt, args);
#else
  std::vprintf(fmt, args);
#endif
}

} // namespace detail

// Public API

inline void begin(Style s) { detail::begin_style(s); }
inline void end(Style s) { detail::end_style(s); }

inline void print(Style s, const char *fmt, ...) {
  detail::begin_style(s);

  va_list args;
  va_start(args, fmt);
  detail::vprint_impl(fmt, args);
  va_end(args);

  detail::end_style(s);
}

inline void print(const char *fmt, ...) {
  // Convenience: plain style
  va_list args;
  va_start(args, fmt);
  detail::vprint_impl(fmt, args);
  va_end(args);
}

inline void refresh_if_needed() {
#ifdef BUILD_WITH_NCURSES
  refresh();
#endif
}

inline int read_char() {
#ifdef BUILD_WITH_NCURSES
  return ::getch();
#else
  return std::cin.get();
#endif
}

inline void setup_screen() {
#ifdef BUILD_WITH_NCURSES
  initscr();            // Initialize the library
  cbreak();             // Line buffering disabled
  keypad(stdscr, TRUE); // Enable special keys
  noecho();             // Don't show the key being pressed
  scrollok(stdscr, TRUE);
#endif
}

inline void teardown_screen() {
#ifdef BUILD_WITH_NCURSES
  endwin();
#endif
}

inline void clear_screen() {
#ifdef BUILD_WITH_NCURSES
  ::clear();
#else
  // ANSI escape sequence: clear screen + move cursor to top
  std::printf("\033[2J\033[H");
  std::fflush(stdout);
#endif
}

} // namespace term