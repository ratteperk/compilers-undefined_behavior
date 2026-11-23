(prog (x)
  (setq x 10)
  (eval '(setq x (plus x 5)))
  x)

(setq fromEval (eval '(lambda (x) (plus x 1))))
(fromEval 9)

(func makeEvalAdder (n)
  (eval '(lambda (x) (plus n x))))
(setq add5 (makeEvalAdder 5))
(add5 7)

(func applyEval (f code) (f code))
(applyEval eval '(plus 3 4))

(func early ()
  (eval '(return 7))
  99)
(early)

(prog (i)
  (setq i 0)
  (while true
    (prog ()
      (setq i (plus i 1))
      (eval '(break))))
  i)

(eval '(prog ()
  (func cube (x) (times x (times x x)))
  (cube 3)))

(prog (i)
  (setq i 0)
  (eval (prog () (setq i (plus i 1)) '(plus i 10))))
