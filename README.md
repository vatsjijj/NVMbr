# NVMbr
## What is NVMbr?
NVMbr stands for **"NVMbr Virtual Machine"** which is a **dynamically typed** programming language, with unique syntax, and written on top of a completely _custom_ VM.

## Features
- REPL
- Concurrent-Mark-Sweep Garbage Collection
- Recursion
- **No for or while loops**
- If statements
- Logic statements, such as "and" and "or"
- First class functions
- Functions
- Native functions
- Variables
- Classes
- Inheritance

## Samples
```
% Hello world, obviously.
puts "Hello, world!".
```
```
% Simple recursive function, imagine
% something like a for or while loop.

func recurse(n) do
  puts n.
  
  if (n > 0)
    return recurse(n - 1).
end

recurse(10).
```
```
% Simple fib.

func fib(n) do
  if (n < 2) return n.
  return fib(n - 2) + fib(n - 1)
end

set start <- clock().
puts fib(35).
puts clock() - start.
```