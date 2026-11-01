(func makeAdder (n)
    (lambda (x) (plus x n))
)

(setq add5 (makeAdder 5))
(setq add10 (makeAdder 10))

(add5 3)
(add10 7)

(func multiplyBy (a)
    (lambda (b)
        (lambda (c) (times (times a b) c))
    )
)

(((multiplyBy 2) 3) 4)
