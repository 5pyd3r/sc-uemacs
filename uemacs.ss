(module
 (init-screen raw-mode no-raw-mode
  screen-resize! screen-rows screen-cols
  ue-winch? ue-char-ready? ue-peek-char ue-read-char
  ue-write-char ue-display-string ue-flush
  move-cursor-up move-cursor-right move-cursor-left move-cursor-down
  scroll-reverse clear-eol clear-eos clear-screen
  carriage-return line-feed
  bell pause wait)
; screen state
 (define cols)
 (define rows)
 (define cursor-col)
 (define the-unread-char)
 (define winch)

; we use terminfo routines directly, rather than going through curses,
; because curses requires initscr(), which clear the screen, discarding
; the current context. this is a shell, not a full-screen user interface.
 (define init-term (foreign-procedure "(cs)ue_init_term" () boolean))
 (define $ue-read-char (foreign-procedure "(cs)ue_read_char" (boolean) scheme-object))
 (define $ue-write-char (foreign-procedure "(cs)ue_write_char" (wchar_t) void))
 (define ue-flush (foreign-procedure "(cs)ue_flush" () void))
 (define get-screen-size (foreign-procedure "(cs)ue_get_screen_size" () scheme-object))
 (define raw-mode (foreign-procedure "(cs)ue_raw" () void))
 (define no-raw-mode (foreign-procedure "(cs)ue_noraw" () void))
 (define enter-am-mode (foreign-procedure "(cs)ue_enter_am_mode" () void))
 (define exit-am-mode (foreign-procedure "(cs)ue_exit_am_mode" () void))
 (define nanosleep (foreign-procedure "(cs)ue_nanosleep" (unsigned-32 unsigned-32) void))
 (define pause (foreign-procedure "(cs)ue_pause" () void))
 (define move-cursor-up (foreign-procedure "(cs)ue_up" (integer-32) void))
 (define move-cursor-down (foreign-procedure "(cs)ue_down" (integer-32) void))
 (define $move-cursor-left (foreign-procedure "(cs)ue_left" (integer-32) void))
 (define $move-cursor-right (foreign-procedure "(cs)ue_right" (integer-32) void))
 (define clear-eol (foreign-procedure "(cs)ue_clr_eol" () void))
 (define clear-eos (foreign-procedure "(cs)ue_clr_eos" () void))
 (define $clear-screen (foreign-procedure "(cs)ue_clear_screen" () void))
 (define scroll-reverse (foreign-procedure "(cs)ue_scroll_reverse" (integer-32) void))
 (define bell (foreign-procedure "(cs)ue_bell" () void))
 (define $carriage-return (foreign-procedure "(cs)ue_carriage_return" () void))
 (define line-feed (foreign-procedure "(cs)ue_line_feed" () void))
  (define (screen-resize!)
    (let ([p (get-screen-size)])
      (set! rows (car p))
      (set! cols (cdr p))))

  (define (screen-rows) rows)
  (define (screen-cols) cols)

  (define (init-screen)
    (and (init-term)
         (begin
           (set! cursor-col 0)
           (set! the-unread-char #f)
           (set! winch #f)
           #t)))

  (define (clear-screen)
    ($clear-screen)
    (set! cursor-col 0))

  (define (ue-winch?)
    (and (not the-unread-char)
      (if winch
          (begin (set! winch #f) #t)
          (begin
            (ue-flush)
            (let ([c ($ue-read-char #t)])
              (or (eq? c #t)
                  (begin (set! the-unread-char c) #f)))))))

  (define (ue-char-ready?)
    (if the-unread-char
        #t
        (let f ()
          (ue-flush)
          (let ([c ($ue-read-char #f)])
            (cond
              [(eq? c #f) #f]
              [(eq? c #t) (set! winch #t) (f)]
              [else (set! the-unread-char c) #t])))))

  (define (ue-read-char)
    (if the-unread-char
        (let ([c the-unread-char]) (set! the-unread-char #f) c)
        (let f ()
          (ue-flush)
          (let ([c ($ue-read-char #t)])
            (if (eq? c #t)
                (begin (set! winch #t) (f))
                c)))))

  (define (ue-peek-char)
    (or the-unread-char
        (let ([c (ue-read-char)])
          (set! the-unread-char c)
          c)))

 ; we assume that ue-write-char receives only characters that occupy one
 ; screen cell.  it should never be passed #\return, #\newline, or #\tab.
 ; furthermore, ue-write-char should never be used to write past the end
 ; of a screen line.
  (define (ue-write-char c)
    (set! cursor-col (fx+ cursor-col 1))
    (if (fx= cursor-col cols)
        (begin
          (exit-am-mode)
          ($ue-write-char c)
          (enter-am-mode))
        ($ue-write-char c)))

 ; comments regarding ue-write-char above apply also to ue-display-string
  (define (ue-display-string s)
    (let ([n (string-length s)])
      (do ([i 0 (fx+ i 1)])
          ((fx= i n))
        (ue-write-char (string-ref s i)))))

  (define (carriage-return)
    (set! cursor-col 0)
    ($carriage-return))

  (define (move-cursor-right n)
    (cond
      [(fx< (fx+ cursor-col n) cols)
       ($move-cursor-right n)
       (set! cursor-col (fx+ cursor-col n))]
      [else
       (move-cursor-down (quotient (fx+ cursor-col n) cols))
       (let ([new-cursor-col (remainder (fx+ cursor-col n) cols)])
         (if (fx>= new-cursor-col cursor-col)
             (move-cursor-right (fx- new-cursor-col cursor-col))
             (move-cursor-left (fx- cursor-col new-cursor-col))))]))

  (define (move-cursor-left n)
    (when (and (fx= cursor-col cols) (fx> n 0))
      (set! n (fx- n 1))
      (set! cursor-col (fx- cursor-col 1)))
    (cond
      [(fx<= n cursor-col)
       ($move-cursor-left n)
       (set! cursor-col (fx- cursor-col n))]
      [else
       (move-cursor-up (fx1+ (quotient (fx- n cursor-col 1) cols)))
       (let ([new-cursor-col (remainder
                               (fx- cols (remainder (fx- n cursor-col) cols))
                               cols)])
         (if (fx>= new-cursor-col cursor-col)
             (move-cursor-right (fx- new-cursor-col cursor-col))
             (move-cursor-left (fx- cursor-col new-cursor-col))))]))

  (define wait
    (lambda (ms)
      (unless (or (<= ms 0) (ue-char-ready?))
        (nanosleep 0 (* 10 1000 1000)) ; 10ms granularity is best we can assume
        (wait (- ms 10))))))

(define $uemacs
  (lambda ()
    #f))
               
(define ue-run
  (lambda (fns)
    (printf "uemacs initilized\n")
    ($uemacs)))

;; (scheme-start
;;  (lambda fns
;;    (ue-run fns)))
