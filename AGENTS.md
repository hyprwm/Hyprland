# AGENTS.md

## Review guidelines

- Prioritize correctness, lack of regressions, performance, API stability, and code clarity and readability.
- For performance-sensitive paths, flag obvious algorithmic regressions or slow paths.
- For tests, flag missing coverage for changed or new behavior, as long as the testing framework is capable of testing it.
- For config changes (new / removed options) remind the author to make a separate wiki PR if they haven't linked one yet.
- Flag silent config breakage: e.g. changing an existing option's behavior. This is not allowed.
- Flag bad config style that breaks this project's style guidelines (further below) and suggest fixes.
- Flag bad code approaches that break this project's core code guidelines (further below) and suggest improvements.

## Style guidelines

- Code must be clang-formatted according to `.clang-format`.
- single-line if and else statements must come without braces. This rule applies only to if / else, not do / while / other.
- Avoid function bodies in headers as much as possible.
- Avoid namespace {} in source files to mark local functions. Prefer `static`.
- Prefer guards in functions and loops: `if (!cond) continue;`
- Prefer forward-declaration in headers to inclusion.
- Leave a stray `,` at the end of brace-enclosed lists to make formatting easier to read.
- Leave a `;` inside empty function bodies for formatting.
- Naming conventions:
 - class: `CMyClass`
 - struct: `SMyStruct`
 - interface: `IMyInterface`
 - class (not struct) member variables: `m_variable`
- Do not use absolute includes from `src/` in headers: instead of `#include "a/b.hpp"` use `#include "../a/b.hpp"` for example. Protocol headers do not require this.

## Core code guidelines

- Stick to good code practices:
 - Avoid complex classes / functions, prefer SRP.
 - Consider using an observer pattern via hyprutils Signals where appropriate.
 - Consider classic OOP patterns where appropriate: Strategy, Singleton, Proxy, etc.
 - Watch out for typical bad practices in code: feature envy, LSP, etc.
 - Use templating and inheritance to clean up code where appropriate.
 - For obtaining singletons, use a `UP<CClass>& myClass();` pattern inside a namespace. This can be implemented in source as making and returning a static ptr.
- Do not, under any circumstance:
 - `using namespace std;`
 - leave uninitialized primitives (int, float, etc)
- Avoid, unless absolutely necessary:
 - the C standard library. Use the C++ STL.
 - `malloc` / `free` / etc
 - C-style pointers. Use SP<> WP<> and UP<> from hyprutils. These are Shared, Weak and Unique pointers respectively. C-style pointers may be used in select scenarios (e.g. destroying fns, where it's impossible to make a mistake) but everywhere else must not be used unless necessary.
 - C-style casts. Use rc<>, sc<>, or cc<> from hyprutils. These are shorthands to equivalent C++ casts.
- Avoid:
 - violating clang-tidy (`.clang-tidy`)
 - manual C-style cleanup: `some_c_thing_new()` and `some_c_thing_free()` can be wrapped.
- Make sure to write tests for code which our Unit (`tests/`) or Integration (`hyprtester/`) tests can test.
