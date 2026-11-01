(func forecast (f time value)
  (cond (equal 0 time) value (forecast f (minus time 1) (f value)))
)
(forecast (lambda (x) (times 2 x)) 10 1)
