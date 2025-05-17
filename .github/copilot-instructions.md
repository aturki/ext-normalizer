# GitHub Copilot Instructions

These instructions define how GitHub Copilot should assist with this project. The goal is to ensure consistent, high-quality code generation aligned with our conventions, stack, and best practices.

## 🧠 Context

- **Project Type**: PHP extension
- **Language**: C
- **Framework / Libraries**: PHP
- **Architecture**: Modular

## 🔧 General Guidelines

- Use idiomatic, portable C.
- Write small, reusable functions—avoid deeply nested logic.
- Use descriptive names; avoid single-letter variables except in loops.
- Always define header files (`.h`) for public interfaces.
- Use `const` and `static` appropriately for scope and immutability.
- Stick to consistent formatting (e.g., `clang-format` or GNU style).
- Prioritize memory safety and avoid undefined behavior.

## 📁 File Structure

Use this structure as a guide when creating or updating files:

```text
.lldb/
src/
  attributes/
  normalizer/
tests/
```

Ignore all files in the `.history` directory.

## 🧶 Patterns

### ✅ Patterns to Follow
- Use modular design with `.c` and `.h` pairs.
- Encapsulate functionality behind clean interfaces.
- Prefer stack memory where possible; use `malloc` only when needed.
- For embedded or low-level apps, abstract hardware using driver layers.
- Use preprocessor macros responsibly (`#define`) and prefer `const` or `enum` for constants.
- Centralize configuration in `config.h`.

### 🚫 Patterns to Avoid
- Don’t use `goto` unless absolutely necessary (e.g., error cleanup).
- Avoid global variables unless required (e.g., ISR flags).
- Don’t write logic directly in `main()`.
- Avoid tight coupling between modules.
- Don't ignore return values of system/library calls.

## 🧪 Testing Guidelines
- Tests are in the tests directory
- Tests should be written in PHP
- To run the test suite, run `make test` in the root directory.
- Test output should be in the `tests/` directory. look for files with extensions `.out`, `.log`.

## 🧩 Example Prompts

- `Copilot, write a C function that reverses a string in-place.`
- `Copilot, create a header and implementation file for a circular buffer library.`
- `Copilot, write a Makefile to compile all .c files in the src directory.`
- `Copilot, implement a timer interrupt handler for an STM32 microcontroller.`
- `Copilot, write unit tests for the parse_config() function using Unity.`

## 🔁 Iteration & Review

- Copilot output should be reviewed and validated via static analysis or compiler warnings.
- Ensure code complies with memory safety and boundary checks.
- Use comments to clarify intent and guide Copilot for better suggestions.

## 📚 References

- [ISO C Standard (C17 Draft)](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n2310.pdf)
- [GNU C Library Documentation](https://www.gnu.org/software/libc/manual/)
- [Modern C by Jens Gustedt (Open Access)](https://gustedt.gitlabpages.inria.fr/modern-c/)
- [Valgrind Memory Debugger](https://valgrind.org/)
- [ClangFormat Style Options](https://clang.llvm.org/docs/ClangFormatStyleOptions.html)
- [PHP Extension Development](https://www.phpinternalsbook.com/)
- [PHP Internals Mailing List](https://externals.io/)
- [PHP Open Telemetry Extension](https://github.com/open-telemetry/opentelemetry-php-instrumentation)
- [PHP Source Code Repository](https://github.com/php/php-src)
