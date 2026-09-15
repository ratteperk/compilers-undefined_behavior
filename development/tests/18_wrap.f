(func wrap (f g)
  (lambda (x) (f (g x)))
)
((wrap (lambda (x) (plus 2 x)) (lambda (x) (times 2 x))) 2)
