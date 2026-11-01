(prog ()
  (func fact (n)
    (cond (lesseq n 1)
          1
          (times n (fact (minus n 1)))))
  (fact 5))
