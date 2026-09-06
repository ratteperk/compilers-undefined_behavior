#### Блок 1: Базовый парсинг и литералы

*   **`tests/01_literals.f`**
    ```lisp
    +42
    -3.14
    true
    false
    null
    ```
    **`tests/01_literals.out`**
    ```text
    42
    -3.14
    true
    false
    null
    ```

*   **`tests/02_nested_lists.f`**
    ```lisp
    (quote (1 (2 3) 4))
    ```
    **`tests/02_nested_lists.out`**
    ```text
    (1 (2 3) 4)
    ```

---

#### Блок 2: Арифметика и логика

*   **`tests/03_math.f`**
    ```lisp
    (plus 10 (times 2 3))
    ```
    **`tests/03_math.out`**
    ```text
    16
    ```

*   **`tests/04_logic.f`**
    ```lisp
    (and (less 1 5) (equal 3 3))
    ```
    **`tests/04_logic.out`**
    ```text
    true
    ```

*   **`tests/05_div_by_zero.f` (Негативный тест)**
    ```lisp
    (divide 10 0)
    ```
    **`tests/05_div_by_zero.err`**
    ```text
    EXIT_CODE != 0
    ```
    *(Спецификация, стр. 4: «If the current value of an argument doesn’t meet the function requirements, the whole program stops execution»).*

---

#### Блок 3: Списки (`head`, `tail`, `cons`)

*   **`tests/06_head_tail.f`**
    ```lisp
    (head (tail (quote (1 2 3))))
    ```
    **`tests/06_head_tail.out`**
    ```text
    2
    ```

*   **`tests/07_cons.f`**
    ```lisp
    (cons 0 (quote (1 2)))
    ```
    **`tests/07_cons.out`**
    ```text
    (0 1 2)
    ```

---

#### Блок 4: Управляющие конструкции (`prog`, `cond`, `while`)

*   **`tests/08_prog_execution.f`**
    ```lisp
    (prog (x y) (setq x 5) (setq y 10) (plus x y))
    ```
    **`tests/08_prog_execution.out`**
    ```text
    15
    ```
    *(Тест проверяет валидность вариативного списка выражений в `prog`).*

*   **`tests/09_cond_three_args.f`**
    ```lisp
    (cond (greater 5 10) 1 0)
    ```
    **`tests/09_cond_three_args.out`**
    ```text
    0
    ```

*   **`tests/10_cond_two_args.f`**
    ```lisp
    (cond (less 10 5) 1)
    ```
    **`tests/10_cond_two_args.out`**
    ```text
    null
    ```
    *(Спецификация, стр. 3: «If the result... is false and there is no third argument... result of the whole form is null»).*

*   **`tests/11_while_loop.f`**
    ```lisp
    (prog (i) (setq i 0) (while (less i 5) (setq i (plus i 1))) i)
    ```
    **`tests/11_while_loop.out`**
    ```text
    5
    ```

---

#### Блок 5: Функции, контекст и краевые случаи

*   **`tests/12_named_func.f`**
    ```lisp
    (prog () (func sqr (x) (times x x)) (sqr 4))
    ```
    **`tests/12_named_func.out`**
    ```text
    16
    ```
    *(Тест проверяет парсинг пустого списка параметров `()`, ломающего грамматику со стр. 6).*

*   **`tests/13_lambda_call.f`**
    ```lisp
    ((lambda (x y) (minus x y)) 10 3)
    ```
    **`tests/13_lambda_call.out`**
    ```text
    7
    ```
    *(Тест проверяет вычисление составного выражения в позиции вызываемой функции).*

*   **`tests/14_scoping_undefined_behavior.f` (Тест на затенение/мутацию)**
    ```lisp
    (prog (a) 
      (setq a 1) 
      (func test () (setq a 2)) 
      (test) 
      a)
    ```
    **`tests/14_scoping_undefined_behavior.out`**
    ```text
    1
    ```
    *(Если интерпретатор реализует Shadowing по правилу `func` со стр. 2 — вывод `1`. Если мутацию по правилу `setq` — вывод `2`).*

*   **`tests/15_recursion.f`**
    ```lisp
    (prog () 
      (func fact (n) 
        (cond (lesseq n 1) 
              1 
              (times n (fact (minus n 1))))) 
      (fact 5))
    ```
    **`tests/15_recursion.out`**
    ```text
    120
    ```

---

### 3. Как этот тестовый набор исполняется (Автоматизация)

Тестирование компиляторов не запускается руками. Для этого пишется скрипт (например, на Bash или Python), который обходит папку `tests/`:

1. Берет файл `tests/XX_name.f`.
2. Передает его исполняемому файлу интерпретатора: `./f_interpreter < tests/XX_name.f > temp.out`.
3. Утилитой `diff` сравнивает полученный `temp.out` с эталонным `tests/XX_name.out`.
4. Если `diff` пустой — тест пройден (PASS). Если вывод отличается — тест провален (FAIL).

Пример минимальной команды проверки одного теста в терминале:
```bash
./f_interpreter < tests/03_math.f | diff -u - tests/03_math.out
```
Отсутствие вывода команды означает полное соответствие поведения интерпретатора спецификации теста.