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

static int eeputc(int c) {
  return putchar(c);
}

int s_ue_init_term(void) {
  int errret;

  if (init_status != -1) return init_status;

  if (isatty(STDIN_FD)
      && isatty(STDOUT_FD)
      && setupterm(NULL, STDOUT_FD, &errret) != ERR
/* assuming here and in our optproc definitions later that the names of
   missing capabilities are set to NULL, although this does not appear
   to be documented */
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

void s_ue_clear_screen(void) {
  tputs(clear_screen, 1, eeputc);
}

void S_uemacs_init(void) {
  Sforeign_symbol("(cs)ue_init_term", (void *)s_ue_init_term);
  Sforeign_symbol("(cs)ue_clear_screen", (void *)s_ue_clear_screen);
}
