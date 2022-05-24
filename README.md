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

## Goals
- Library support
- Header file support (`.nvmh`)
- Improved syntax
- Module support
- Functional style of programming
- Arrays
- Tuples
- Lists
- Fault-Tolerant

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
  return fib(n - 2) + fib(n - 1).
end

set start <- clock().
puts fib(35).
puts clock() - start.
```
```
% Superclass example.

class Doughnut [
  cook() do
    puts "Dunk in the fryer.".
    this:finish("sprinkles").
  end

  finish(ingredient) do
    puts "Finish with " + ingredient.
  end
]

class Cruller < Doughnut [
  finish(ingredient) do
    super:finish("icing").
  end
]
```
```
% Match statement.

set x <- 3.

match (x) do
  case 1 ->
    puts "I'm number 1!".
  case 2 ->
    puts "...I don't like that 1 guy.".
  case 3 ->
    puts "Those two are annoying!".
  ~ ->
    puts "...I'm the default.".
end
```

## Installation
### Linux
Ensure at least GCC 10.3.0, Make 4.3, and Git is installed.

```
git clone https://www.github.com/vatsjijj/NVMbr.git
cd NVMbr/
make
sudo make install
```
You can run NVMbr by typing `nvmbrc` in your terminal.

You can uninstall NVMbr by running `sudo make uninstall`.
### Windows
Ensure [MinGW-w64](https://www.mingw-w64.org/), [Make](https://community.chocolatey.org/packages/make), and [Git](https://git-scm.com/download/win) is installed.

**In Command Prompt**

```
git clone https://www.github.com/vatsjijj/NVMbr.git
cd NVMbr
make
```
You can run NVMbr by typing `nvmbrc.exe` in your Command Prompt.

<br><hr><br>
**NVMbr is a dead project. You can use Udin instead.**
