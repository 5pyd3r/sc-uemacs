#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wchar.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <curses.h>
#include <term.h>
#include <termios.h>
#include <signal.h>
#include <locale.h>
#include <limits.h>

#include <scheme.h>

static int init_status = -1;

static void handle_sigwinch(int sig);

static int winched = 0;
static void handle_sigwinch(__attribute__((unused)) int sig) {
  winched = 1; 
}

#define STDIN_FD 0
#define STDOUT_FD 1

static int disable_auto_margin = 0, avoid_last_column = 0;
static locale_t term_locale;
static mbstate_t term_in_mbs;
static mbstate_t term_out_mbs;

static int ueputc(int c) {
  return putchar(c);
}

#define LIST1(x) Scons(x, Snil)
#define LIST2(x, y) Scons(x, LIST1(y))
#define LIST3(x, y, z) Scons(x, LIST2(y, z))

static ptr S_strerror(int errnum) {
  ptr p;
  char *msg;

  p = (msg = strerror(errnum)) == NULL? Sfalse: Sstring(msg);

  return p;
}

static void do_error(const char *who, const char *s, ptr args) {

}

static void S_error(const char *who, const char *s) {
  do_error(who, s, Snil);
}

static void S_error1(const char *who, const char *s, ptr x) {
  do_error(who, s, LIST1(x));
}

static void S_error2(const char *who, const char *s, ptr x, ptr y) {
  do_error(who, s, LIST2(x, y));
}

static void S_error3(const char *who, const char *s, ptr x, ptr y, ptr z) {
  do_error(who, s, LIST3(x, y, z));
}

int s_ue_init_term(void) {
  int errret;

  if (init_status != -1) return init_status;

  if (isatty(STDIN_FD)
      && isatty(STDOUT_FD)
      && setupterm(NULL, STDOUT_FD, &errret) != ERR
      && cursor_up
      && cursor_down
      && cursor_left
      && cursor_right
      && clr_eol
      && clr_eos
      && clear_screen
      && scroll_reverse
      && carriage_return) {

    struct sigaction act;

    sigemptyset(&act.sa_mask);

    act.sa_flags = 0;
    act.sa_handler = handle_sigwinch;
    sigaction(SIGWINCH, &act, (struct sigaction *)0);

    term_locale = newlocale(LC_ALL_MASK, "", NULL);
    memset(&term_out_mbs, 0, sizeof(term_out_mbs));
    memset(&term_in_mbs, 0, sizeof(term_in_mbs));

    init_status = 1;
  } else {
    init_status = 0;
  }

  return init_status;
}

/* returns char, eof, #t (winched), or #f (nothing ready), the latter
   only if blockp is false */
static ptr s_ue_read_char(int blockp) {
  ptr msg;
  int fd = STDIN_FD;
  int n;
  char buf[1];
  wchar_t wch;
  size_t sz;
  locale_t old_locale;

  do {
    if (winched) {
      winched = 0;
      return Strue;
    }

    if (!blockp) {
      fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
      n = read(fd, buf, 1);
      fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) & O_NONBLOCK);
      if (n < 0 && errno == EWOULDBLOCK) {
        return Sfalse;
      }
    } else {
      n = read(fd, buf, 1);
    }

    if (n == 1) {
      old_locale = uselocale(term_locale);
      sz = mbrtowc(&wch, buf, 1, &term_out_mbs);
      uselocale(old_locale);
      if (sz == 1) {
        return Schar(wch);
      }
    }
  } while ((n < 0 && errno == EINTR) || (n == 1 && sz == (size_t)-2));

  if (n == 0)
    return Seof_object;

  msg = S_strerror(errno);
  S_error1("uemacs", "error reading from console: ~a", msg);

  memset(&term_out_mbs, 0, sizeof(term_out_mbs));
  return Svoid;
}

/* returns a pair of positive integers */
static ptr s_ue_get_screen_size(void) {
  static int ue_rows = 0;
  static int ue_cols = 0;

#ifdef TIOCGWINSZ
  struct winsize ws;
  if (ioctl(STDOUT_FD, TIOCGWINSZ, &ws) == 0) {
    if (ws.ws_row > 0) ue_rows = ws.ws_row;
    if (ws.ws_col > 0) ue_cols = ws.ws_col;
  }
#endif /* TIOCGWINSZ */

  if (ue_rows == 0) {
    char *s, *endp;
    if ((s = getenv("LINES")) != NULL) {
      int n = (int)strtol(s, &endp, 10);
      if (n > 0 && *endp == '\0') ue_rows = n;
    }
    if (ue_rows == 0) ue_rows = lines > 0 ? lines : 24;
  }

  if (ue_cols == 0) {
    char *s, *endp;
    if ((s = getenv("COLUMNS")) != NULL) {
      int n = (int)strtol(s, &endp, 10);
      if (n > 0 && *endp == '\0') ue_cols = n;
    }
    if (ue_cols == 0) ue_cols = columns > 0 ? columns : 80;
  }

  return Scons(Sinteger(ue_rows), Sinteger(ue_cols > 1 && avoid_last_column ? ue_cols - 1 : ue_cols));
}

static struct termios orig_termios;

static void s_ue_raw(void) {
  struct termios new_termios;
  while (tcgetattr(STDIN_FD, &orig_termios) != 0) {
    if (errno != EINTR) {
      ptr msg = S_strerror(errno);
      if (msg != Sfalse)
        S_error1("uemacs", "error entering raw mode: ~a", msg);
      else
        S_error("uemacs", "error entering raw mode");
    }
  }
  new_termios = orig_termios;

  /* essentially want "stty raw -echo".  the appropriate flags to accomplish
    this were determined by studying the gnu/linux stty and termios man
    pages, with particular attention to the cfmakeraw function. */
  new_termios.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|INPCK|ISTRIP
                            |INLCR|IGNCR|ICRNL|IXON);
  new_termios.c_oflag &= ~(OPOST);
  new_termios.c_lflag &= ~(ISIG|ICANON|ECHO|IEXTEN);
  new_termios.c_cflag &= ~(CSIZE|PARENB);
  new_termios.c_cflag |= CS8;
  new_termios.c_cc[VMIN] = 1;
  new_termios.c_cc[VTIME] = 0;

  while (tcsetattr(STDIN_FD, TCSADRAIN, &new_termios) != 0) {
    if (errno != EINTR) {
      ptr msg = S_strerror(errno);
      if (msg != Sfalse)
        S_error1("expeditor", "error entering raw mode: ~a", msg);
      else
        S_error("expeditor", "error entering raw mode");
    }
  }
}

static void s_ue_noraw(void) {
  while (tcsetattr(STDIN_FD, TCSADRAIN, &orig_termios) != 0) {
    if (errno != EINTR) {
      ptr msg = S_strerror(errno);
      if (msg != Sfalse)
        S_error1("expeditor", "error leaving raw mode: ~a", msg);
      else
        S_error("expeditor", "error leaving raw mode");
    }
  }
}

static void s_ue_enter_am_mode(void) {
  if (disable_auto_margin) {
    tputs(enter_am_mode, 1, ueputc);
   /* flush to minimize time span when automatic margins are disabled */
    fflush(stdout);
  } else if (eat_newline_glitch) {
   /* hack: try to prevent terminal from eating subsequent cr or lf.
      assumes we've just written to last column.  probably works only
      for vt100 interpretation of eat_newline_glitch/xn/xenl flag. */
    tputs(cursor_left, 1, ueputc);
    tputs(cursor_right, 1, ueputc);
  }
}

static void s_ue_exit_am_mode(void) {
  if (disable_auto_margin) {
    tputs(exit_am_mode, 1, ueputc);
  }
}

static void s_ue_pause(void) { /* used to handle ^Z */
  fflush(stdout);
  kill(0, SIGTSTP);
}

static void s_ue_nanosleep(unsigned int secs, unsigned int nanosecs) {
  struct timespec ts;
  ts.tv_sec = secs;
  ts.tv_nsec = nanosecs;
  nanosleep(&ts, (struct timespec *)0);
}

static void s_ue_up(int n) {
  while (n--) tputs(cursor_up, 1, ueputc);
}

static void s_ue_down(int n) {
  while (n--) tputs(cursor_down, 1, ueputc);
}

static void s_ue_left(int n) {
  while (n--) tputs(cursor_left, 1, ueputc);
}

static void s_ue_right(int n) {
  while (n--) tputs(cursor_right, 1, ueputc);
}

static void s_ue_clear_eol(void) {
  tputs(clr_eol, 1, ueputc);
}

static void s_ue_clear_eos(void) {
  tputs(clr_eos, 1, ueputc);
}

static void s_ue_clear_screen(void) {
  tputs(clear_screen, 1, ueputc);
}

static void s_ue_scroll_reverse(int n) {
 /* moving up from an entry that was only partially displayed,
    scroll-reverse may be called when cursor isn't at the top line of
    the screen, in which case we hope it will move up by one line.
    in this case, which we have no way of distinguishing from the normal
    case, scroll-reverse needs to clear the line explicitly */
  while (n--) {
    tputs(scroll_reverse, 1, ueputc);
    tputs(clr_eol, 1, ueputc);
  }
}

static void s_ue_bell(void) {
  tputs(bell, 1, ueputc);
}

static void s_ue_carriage_return(void) {
  tputs(carriage_return, 1, ueputc);
}

/* move-line-down doesn't scroll the screen when performed on the last
   line on the freebsd and openbsd consoles.  the official way to scroll
   the screen is to use scroll-forward (ind), but ind is defined only
   at the bottom left corner of the screen, and we don't always know
   where the bottom of the screen actually is.  so we write a line-feed
   (newline) character and hope that will do the job. */
static void s_ue_line_feed(void) {
  putchar(0x0a);
}

static void s_ue_write_char(wchar_t wch) {
  locale_t old; char buf[MB_LEN_MAX]; size_t n;

  old = uselocale(term_locale);
  n = wcrtomb(buf, wch, &term_in_mbs);
  if (n == (size_t)-1) {
    putchar('?');
  } else {
    fwrite(buf, 1, n, stdout);
  }
  uselocale(old);
}

static void s_ue_flush(void) {
  fflush(stdout);
}

void S_uemacs_init(void) {
  Sforeign_symbol("(cs)ue_init_term", (void *)s_ue_init_term);
  Sforeign_symbol("(cs)ue_read_char", (void *)s_ue_read_char);
  Sforeign_symbol("(cs)ue_write_char", (void *)s_ue_write_char);
  Sforeign_symbol("(cs)ue_flush", (void*)s_ue_flush);
  Sforeign_symbol("(cs)ue_get_screen_size", (void *)s_ue_get_screen_size);
  Sforeign_symbol("(cs)ue_raw", (void *)s_ue_raw);
  Sforeign_symbol("(cs)ue_noraw", (void *)s_ue_noraw);
  Sforeign_symbol("(cs)ue_enter_am_mode", (void *)s_ue_enter_am_mode);
  Sforeign_symbol("(cs)ue_exit_am_mode", (void *)s_ue_exit_am_mode);
  Sforeign_symbol("(cs)ue_pause", (void *)s_ue_pause);
  Sforeign_symbol("(cs)ue_nanosleep", (void *)s_ue_nanosleep);
  Sforeign_symbol("(cs)ue_up", (void *)s_ue_up);
  Sforeign_symbol("(cs)ue_down", (void *)s_ue_down);
  Sforeign_symbol("(cs)ue_left", (void *)s_ue_left);
  Sforeign_symbol("(cs)ue_right", (void *)s_ue_right);
  Sforeign_symbol("(cs)ue_clr_eol", (void *)s_ue_clear_eol);
  Sforeign_symbol("(cs)ue_clr_eos", (void *)s_ue_clear_eos);
  Sforeign_symbol("(cs)ue_clear_screen", (void *)s_ue_clear_screen);
  Sforeign_symbol("(cs)ue_scroll_reverse", (void *)s_ue_scroll_reverse);
  Sforeign_symbol("(cs)ue_bell", (void *)s_ue_bell);
  Sforeign_symbol("(cs)ue_carriage_return", (void *)s_ue_carriage_return);
  Sforeign_symbol("(cs)ue_line_feed", (void *)s_ue_line_feed);
}
