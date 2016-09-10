(define init-term)
(define clear-screen)

(define ue-init
  (lambda ()
    (set! init-term (foreign-procedure "(cs)ue_init_term" () boolean))
    (set! clear-screen (foreign-procedure "(cs)ue_clear_screen" () void))))

(define ue-run
  (lambda (fns)
    (init-term)
    (clear-screen)
    (printf "uemacs initilized\n")))

(scheme-start
 (lambda fns
   (ue-init)
   (ue-run fns)))
