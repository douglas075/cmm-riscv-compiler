# Project Report: A C-- Compiler Targeting RISC-V

*See [../README.md](../README.md) for the short summary, feature table, and verified example. This document goes into more depth on the design and my individual work.*

## 1. Background and Goals

This project was the four-part final project for *Compiler Design* (CSIE3230), National Taiwan University, Fall 2025 (114-1), taught by Prof. Ding-Yong Hong. The assignment specified a small, deliberately restricted subset of ANSI C called **C--**, and the project was split into four milestones, each graded separately and building directly on the previous one's code:

1. Lexer (flex)
2. Parser + AST (bison)
3. Semantic analysis + symbol table
4. Code generation, targeting RISC-V 64-bit assembly (runnable under QEMU)

The goal across all four stages was correctness over sophistication: get every construct in the language specification compiling to RISC-V assembly that actually runs under QEMU, within a one-month-per-milestone timeframe. Classic optimizations (constant folding, dead-code elimination, register allocation beyond simple round-robin) were explicitly out of scope for the course.

I worked on this with one teammate; the two of us split the four milestones between us, with some joint debugging sessions near each deadline.

## 2. System Design, Stage by Stage

**Lexer.** Hand-written regular expressions in flex recognize C--'s reserved words, identifiers, integer/float/string literals, and operators, following the course's language spec closely.

**Parser.** A bison grammar builds an abstract syntax tree (AST) directly during parsing — there's no separate "parse tree to AST" lowering step. Operator precedence and the classic dangling-`else` ambiguity are resolved with bison's precedence declarations.

**Semantic analysis.** A single recursive AST walk performs type-checking (int/float promotion rules, argument-count and argument-type checking against each function's signature, array-dimension checking) and manages a symbol table. The symbol table is a fixed-size hash table where each bucket holds a linked list of same-name entries (for shadowing across nested scopes) as well as a per-scope-level linked list (so an entire scope can be torn down in one pass when a block exits).

**Code generation.** This is the stage I spent the most time on. There is no intermediate representation: the AST is walked once, and each node emits RISC-V instructions directly into the output assembly file. A few project-wide design decisions shaped this stage:

- **Calling convention.** The team decided to pass *all* function arguments on the stack, rather than using RISC-V's `a0`–`a7` argument registers. This was a deliberate trade-off: it made calls with more than 8 arguments (which the language spec allows and which I specifically wrote a test for) trivial to handle uniformly, at the cost of more verbose generated code than a register-passing convention would produce.
- **Stack frame layout.** Each function's frame size is computed at compile time, but rather than backpatching it into the prologue instruction directly, it's stored as a named constant in the `.data` section and loaded at runtime before the stack pointer is adjusted. I designed and implemented this prologue/epilogue scheme (see below).
- **Register allocation.** Registers are handed out by a simple round-robin scheme, not liveness analysis or graph coloring. This is correct for the programs the language can express (expressions don't get deep enough to exhaust the register file in practice) but is the most obvious place this project would need real work to become production-grade.

## 3. My Individual Contributions

Code generation was a joint effort; the pieces below are the ones I personally designed and implemented, each verifiable against the project's commit history:

- **Global variable declarations and initialization.** I wrote the logic that turns a global `int`/`float`/array declaration — with or without an initializer — into the correct `.data` section directive (`.word`, `.float`, or `.zero` for an uninitialized array), and I separately found and fixed a bug where global symbols were being emitted into the data section but never actually entered into the symbol table, which meant later references to them would fail to resolve.
- **String constant pooling.** `write()` can take a string literal, and the convention we used was to collect every string literal encountered during code generation and emit them as a single `.rodata` block with generated labels at the very end of the output, rather than inline at each use site. I implemented the collection and emission logic, including a couple of follow-up fixes for edge cases in how the constants were quoted.
- **Function prologue and epilogue.** I designed and implemented the entry/exit sequence for every function: saving the return address and caller's frame pointer, computing the new stack pointer by loading a runtime constant for that function's frame size, and the matching teardown on the way out. This is the piece of codegen that every other piece of generated code depends on being correct.
- **Multi-dimensional array indexing.** I rewrote the subscript address calculation for arrays with more than one dimension. The original version accumulated the offset in the wrong order across dimensions; I reworked it to accumulate a running byte offset, multiplying by the stride of each dimension from the innermost outward, and in the process also caught and fixed a bug where the wrong AST node (an already-consumed node instead of the array identifier) was being passed into the subscript calculator — a bug that would silently miscompute addresses rather than crash, which made it the kind of thing I was specifically trying to catch with targeted test cases.
- **Test design — parser grammar failures.** I wrote all three of the dedicated "should fail to parse" tests for array declarations: `stages/hw3-semantic/tests/pfail1.c` (`int c[2.5]` — a non-integer dimension literal), `pfail2.c` (`int b[a]`, where `a` is a variable — a non-constant dimension expression), and `pfail3.c` (`int a[]` — an empty dimension used outside a function parameter list, where the language only allows omitting the first dimension). These map one-to-one onto the three "parse fail" categories the project documented for its grammar. Notably, `pfail1.c` wasn't written from scratch — I took an earlier, bundled test that mixed the "non-integer literal" and "non-constant expression" failure modes into a single file (`int c[2.5]; int a = 10; int b[a];`) and split it into two, because those two failures exercise genuinely different parts of the grammar: the constant-expression rule (`cexpr`) only reduces arithmetic over integer literals, so a bare identifier like `a` simply never matches it, which is a structurally different kind of rejection than a literal being the wrong type. Leaving them bundled would have made it impossible to tell, from a test failure alone, which of the two checks had actually broken.
- **Test design — code generation.** For the final milestone I wrote five test programs and, in an earlier milestone, a regression-test runner script. Each test program was chosen to isolate one feature I thought was likely to have an off-by-one or ordering bug: a 10-argument function call (to stress the stack-based calling convention past the point where a register convention would even apply), a 2×3 two-dimensional array traversed in both row-major and column-major order (to catch a stride-direction bug like the one described above), nested-scope variable shadowing (global vs. outer-block vs. inner-block `x`), floating-point arithmetic combined with a global float constant, and an in-place array sort (to exercise array reads *and* writes together, with a loop-carried dependency). I treated writing these as part of the actual engineering work, not an afterthought after the "real" code was done.

## 4. Results

- Final grade for the project: **A+.** I want to be precise about what that signals: the grading curve in this course was generous, and most of my classmates also received A+, so this reflects clearing the bar for a working compiler rather than standing out among peers.
- For this write-up, I rebuilt the final-stage compiler from source in a clean environment (flex + bison + gcc) and re-ran it against one of my own test programs (`sum_many.c`, shown in the README). It built without errors or warnings, and the generated assembly for the test's `if (x == 38)` check lowers exactly as expected — a comparison, a `seqz`, and a conditional branch to the correct label for each side of the `if`/`else`. I did not have a RISC-V toolchain and QEMU available in this environment to actually execute the generated binary, so I'm not claiming a freshly-verified runtime output here; the original submission was executed and graded under the course's QEMU-based workflow, which is what the A+ reflects.

## 5. Reflection

Two things stuck with me from this project. First, having to invent our own calling convention and stack frame layout — rather than just following gcc's — made the cost of those decisions concrete in a way reading about ABIs never had: choosing to pass everything on the stack was the *simple* choice, and I could see directly in the generated assembly how much more verbose it made every call site compared to what a register-passing convention would produce. Second, round-robin register allocation works until it doesn't, and watching it start to strain on nothing more complex than a 10-argument function call made the case for real liveness analysis and graph-coloring allocation very viscerally — which is part of why I'm interested in compiler optimization and systems work (including how LLM inference runtimes think about register/memory allocation) as a direction for graduate study.

## Notes

- Full source code is included in this repository under [`../stages/`](../stages), organized by milestone.
- Course: Compiler Design (CSIE3230), National Taiwan University, Fall 2025 (114-1), Prof. Ding-Yong Hong. Project specification ("C-- Language") and milestone assignments were provided by the instructor.
- **AI/LLM usage**: essentially none during the original implementation. This write-up was drafted with AI assistance from the author's own notes and the project's commit history; the compiler code itself was not AI-assisted.
