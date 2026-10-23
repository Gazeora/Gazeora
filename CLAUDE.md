# CLAUDE.md

These rules are mandatory for every task in this repository. Follow them strictly. If a rule conflicts with a request, ask before deviating.

---

## 1. Object-Oriented Programming (Required)

All code MUST be object-oriented. Do not write procedural scripts or loose functions unless the language or framework forces it (e.g. an entry-point `main`).

### Core OOP principles
- **Encapsulation**: Keep fields `private`/`protected`. Expose behavior through methods, not raw data. No public mutable state.
- **Abstraction**: Program against interfaces/abstract classes, never concrete implementations.
- **Inheritance**: Use only for true "is-a" relationships. Prefer **composition over inheritance**.
- **Polymorphism**: Replace `if/else` or `switch` chains on types with polymorphic classes or strategies.

### SOLID (non-negotiable)
- **S — Single Responsibility**: One class = one reason to change. Split classes that do more than one job.
- **O — Open/Closed**: Extend behavior by adding new classes, not by editing existing ones.
- **L — Liskov Substitution**: Subclasses must be fully substitutable for their base types.
- **I — Interface Segregation**: Small, focused interfaces. No "fat" interfaces with unused methods.
- **D — Dependency Inversion**: Depend on abstractions. Inject dependencies through constructors; never instantiate collaborators with `new` inside business logic.

### Additional rules
- Follow **DRY**, **KISS**, and **YAGNI**.
- Methods should be short and do one thing (aim for ≤ 20 lines).
- Use meaningful, intention-revealing names for classes, methods, and variables.
- No magic numbers or strings — use constants or enums.
- No global variables or static mutable state.
- Validate inputs and use custom, meaningful exceptions.
- One public class per file; file name matches the class name.

---

## 2. Design Patterns (Required)

Apply an appropriate, well-known design pattern whenever the problem fits one. Do not over-engineer: choose the simplest pattern that solves the problem.

### Use these patterns where applicable

**Creational**
- **Factory Method / Abstract Factory** — when object creation depends on type or configuration.
- **Builder** — for objects with many optional parameters.
- **Singleton** — only for truly global, stateless resources (config, logger). Prefer DI over Singleton.

**Structural**
- **Adapter** — to integrate external/third-party APIs behind your own interface.
- **Decorator** — to add behavior without modifying the original class.
- **Facade** — to simplify access to a complex subsystem.
- **Repository** — for all data access; business logic never talks to the database directly.

**Behavioral**
- **Strategy** — to swap algorithms or replace conditional logic.
- **Observer** — for event-driven notifications.
- **Command** — to encapsulate actions/requests.
- **Template Method** — for shared algorithm skeletons with varying steps.
- **State** — when behavior changes based on an object's state.

### Pattern rules
- When you apply a pattern, name it in a short class-level comment/docstring (e.g. `// Strategy pattern: payment processing`).
- Keep a clear layered architecture: **Controller/Presentation → Service → Repository → Model**.
- Before writing code for a new feature, briefly state which classes and pattern(s) you will use.

---

## 3. Modularity & Directory Structure (Required)

Code MUST always be modular. Never put unrelated logic in the same file.

- Split code into small, focused files. **Create as many files and folders as needed.** More small files are always better than one large file.
- Each module has a single, clear purpose and can be understood on its own.
- Group files by feature/layer in a clean, consistent directory structure.
- Respect the existing directory structure. Place new files where they logically belong; create new folders only when needed and keep naming consistent.
- Keep folder and file names clear and predictable (e.g. `services/`, `repositories/`, `models/`, `controllers/`, `interfaces/`, `utils/`, `config/`).
- Modules communicate only through public interfaces. No circular dependencies.
- Shared/reusable logic goes into its own module, never copy-pasted.
- Keep files short (aim for ≤ 200 lines). If a file grows beyond that, split it.

Example layout (adapt to the language/framework in use):

```
src/
├── config/
├── controllers/
├── services/
├── repositories/
├── models/
├── interfaces/
├── patterns/        # factories, strategies, etc. if not placed per feature
├── exceptions/
└── utils/
tests/
└── (mirrors src/ structure)
```

---

## 4. Comments (Minimal & Simple)

- Write **minimal, short, easy-to-understand comments**. Plain language only.
- Code should explain itself through good names. Comment the **why**, not the **what**.
- One short line per comment where possible. No long paragraphs, no complex explanations.
- Do not comment obvious code (e.g. `// increment i`).
- No commented-out code. Delete it instead.
- Use a brief one-line docstring for public classes and methods only when it adds clarity.

### Good
```
// Retry because the payment API sometimes times out
```

### Bad
```
// This method takes the input parameter which is a list of users and then
// iterates over each user object in the list using a for loop, checking...
```

---

## 5. Git Commit Rules (Strict)

- Commit messages MUST be a **single line only**. No body, no extended description, no bullet points.
- **NEVER** add a `Co-Authored-By:` trailer.
- **NEVER** add "Generated with Claude Code", emojis, or any AI attribution.
- Use Conventional Commits format: `<type>: <short summary>`
  - Types: `feat`, `fix`, `refactor`, `docs`, `test`, `chore`, `style`, `perf`
- Imperative mood, lowercase, no trailing period, max 72 characters.
- Always commit with a plain `-m` flag and nothing else:

```bash
git commit -m "feat: add payment strategy for upi"
```

### Correct
```
feat: add user repository with jpa implementation
fix: handle null token in auth service
refactor: extract notification logic into observer
```

### Incorrect (never do this)
```
feat: add user repository

- Added UserRepository interface
- Added JPA implementation

🤖 Generated with Claude Code
Co-Authored-By: Claude <noreply@anthropic.com>
```

- Do not use HEREDOCs or multi-line strings for commit messages.
- Make small, focused commits — one logical change per commit.

---

## 6. Before Finishing Any Task

- [ ] Code is fully object-oriented and follows SOLID.
- [ ] An appropriate design pattern is used and named.
- [ ] Code is modular and split into small, focused files.
- [ ] Files are placed correctly in the directory structure.
- [ ] Comments are minimal, short, and easy to understand.
- [ ] Dependencies are injected, not hard-coded.
- [ ] No duplicated logic, magic values, or global state.
- [ ] Commit message is a single line with no co-author or attribution.
