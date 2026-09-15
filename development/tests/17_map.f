(func map (f lst)
  (cond (isnull lst) null (cons (f (head lst)) (map f (tail lst))))
)
(map (lambda (x) (plus 1 x)) '(1 2 3 4))
