# Contributing to Gazeora

Thanks for helping. Gazeora gets better from bug reports, game reports and code alike, and you
don't need to write code to help.

## Ways to help

- **Report a problem.** [Open an issue](https://github.com/Gazeora/Gazeora/issues/new/choose)
  and pick the form that fits: bug report, crash or freeze, performance problem, or feature
  request. Each form asks for what's needed to look into it, such as the version, the stream
  settings and `stream.log`.
- **Tell us how a game works.** The **Game compatibility** form records whether a game captures,
  with which graphics API and launcher. Reports that a game works are as useful as ones that
  don't: they build the list of tested games.
- **Ask or talk over an idea** in [Discussions](https://github.com/orgs/Gazeora/discussions)
  before opening an issue, if you're not sure it's a bug or what a feature should look like.
- **Fix or build something.** See below. For anything bigger than a small fix, open an issue or
  a discussion first, so we agree on the approach before you put the time in.

Found a security problem? Don't open an issue:
[report it privately](https://github.com/Gazeora/Gazeora/security/advisories/new).

## Setting up

1. Build Gazeora as [Development](docs/development.md#building) describes: CMake 3.20 or
   newer, and MinGW-w64 or Visual Studio 2022 or newer. The compiler's bitness picks the
   release (x64 or x86).
2. Run the tests with `ctest --test-dir build --output-on-failure`. They run Direct3D on WARP,
   so no GPU is needed.
3. Install the pre-commit hook, which formats C and C++ with clang-format and checks line
   endings, whitespace and license headers before each commit:

   ```powershell
   pip install pre-commit
   pre-commit install
   ```

[Development](docs/development.md) covers building, the tests and the benchmarks, and
[Architecture](docs/architecture.md) the project layout. The
[stream format](docs/stream-format.md) and the other guides in `docs/` describe how each part
works.

## How the code is written

Gazeora follows a few firm rules, so the code stays easy to find your way around:

- **Object-oriented, SOLID.** Behavior lives in classes with private state. Classes depend on
  small interfaces and get their collaborators through the constructor, so they can be tested
  with fakes (see `tests/`). Prefer composition and polymorphism over long `if` or `switch`
  chains on a type.
- **Patterns, by name.** When a well-known pattern fits (Strategy, Adapter, Factory, Observer,
  Repository...), use it and name it in a one-line comment on the class.
- **Small files.** One public class per file, named after it, and files of about 200 lines at
  most. Methods do one thing, in about 20 lines.
- **No magic values or global state.** Constants or enums instead of bare numbers and strings,
  and no mutable statics.
- **Few, short comments.** Names should say *what* the code does; comments say *why*, in one
  plain line.
- **Formatting** is whatever `.clang-format` produces: run `cmake --build build --target format`.

Match the code around your change. When in doubt, look at how a similar part already does it.

## Making a pull request

1. Branch from `master` and keep the change focused: one fix or feature per pull request.
2. Write commits in [Conventional Commits](https://www.conventionalcommits.org) form, one line
   each, lowercase, imperative, no period, at most 72 characters, with no body:

   ```
   feat: add stereo depth to opengl games
   fix: reconnect usb after sleep
   ```

   Types: `feat`, `fix`, `perf`, `refactor`, `docs`, `test`, `chore`, `style`. Make small
   commits, one logical change each.
3. Add tests for new logic where it can run without a GPU or a game, and update the README or
   the guides in `docs/` when users will notice the change, following
   [DOCUMENTATION_RULES.md](DOCUMENTATION_RULES.md).
4. Check that both releases still build if your change touches anything bitness-specific, and
   that `ctest` passes.
5. Open the pull request and fill in its template: what changed, which part, and how you tested
   it (games, graphics APIs, protocol, phone).

A maintainer reviews every pull request. Expect questions and requests for changes; they're part
of getting the change in, not a judgment of it.

## License of contributions

Gazeora is free software under the [GNU GPL version 3](LICENSE) with the additional terms in
[NOTICE](NOTICE), and is also offered under a [commercial license](COMMERCIAL-LICENSE.md). So
that every part of it can be offered both ways, contributors sign the
[Contributor License Agreement](CLA.md) once: on your first pull request, a bot asks you to
post a one-line comment that signs it. In short, you confirm that you have the right to submit
the work, and you agree that:

- it is licensed under the GPL version 3 with the terms in [NOTICE](NOTICE); and
- you also grant Nithin Aashik a perpetual, worldwide, non-exclusive, royalty-free and
  irrevocable license to use, change, sublicense and distribute it under other terms,
  including the commercial license.

You keep the copyright in your work, and can use it however you like. Code from elsewhere must
come under a license compatible with the GPL version 3 that also allows the commercial license,
such as MIT, BSD or Apache 2.0; name it in the pull request, and add it to
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

The Gazeora name and logo aren't covered by the license: see [TRADEMARKS.md](TRADEMARKS.md).
