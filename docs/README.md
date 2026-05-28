# needful-enhanced docs

This directory is the source for [needful.metaeducation.com](https://needful.metaeducation.com),
built with Jekyll and the [just-the-docs](https://just-the-docs.com) theme.

## Page inventory

| File | URL | Topic |
|---|---|---|
| `index.md` | `/` | Homepage, feature table, getting started |
| `setup.md` | `/setup` | Installing needful-enhanced, enabling `NEEDFUL_CPP_ENHANCED` |
| `need.md` | `/need` | `Need(T)` — non-null/non-zero type |
| `option.md` | `/option` | `Option(T)` — Rust-like optional |
| `result.md` | `/result` | `Result(T)` — cooperative error propagation |
| `cast.md` | `/cast` | Cast family: `cast`, `m_cast`, `raw_cast`, etc. |
| `contra.md` | `/contra` | `Contra(T)` / `Sink(T)` / `Init(T)` — contravariant output parameters |
| `known.md` | `/known` | `known(T, expr)` — compile-time type assertion inside macros |
| `comments.md` | `/comments` | `possibly`, `dont`, `heeded`, `unnecessary`, `definitely`, `impossible` |
| `nocast.md` | `/nocast` | `nocast`, `needful_nocast_0` — C/C++ portability bridges |
| `faq.md` | `/faq` | Frequently asked questions and design rationale |
| `internals-template-cast.md` | `/internals/template-cast-operator` | Why SinkWrapper uses `c_cast(T*, c_cast(void*, u))` |

`nav_order` in each page's YAML front matter controls the sidebar order.

## Embedded compile-time tests

Every code example in these pages that demonstrates a type-checked behavior
is also a live compile-time test.  Tag a fenced block with an HTML comment on
the immediately preceding line:

```markdown
<!-- doctest: positive-test -->
```cpp
// Full .cpp source — compiled and run; must exit 0
```

<!-- doctest: negative-test -->
```cpp
// MATCH-ERROR-TEXT: no viable conversion   <- Clang
// MATCH-ERROR-TEXT: cannot convert         <- GCC / MSVC
// Full .cpp source — must FAIL to compile
```
```

The `match=` shorthand injects `MATCH-ERROR-TEXT` comments automatically:

```markdown
<!-- doctest: negative-test match="no viable conversion|cannot convert" -->
```cpp
...
```
```

The HTML comment is **invisible in rendered output** — Jekyll ignores it.
Only `tools/extract-doctests.py` reads it to extract the block into
`tests/doctest-extracted/{positive,negative}/`.  CMake runs the extractor
at configure time; see `tests/README.md` for the full workflow.

## Building the site locally

```sh
gem install bundler
cd docs
bundle init
bundle add jekyll just-the-docs
bundle exec jekyll serve
```

Then open `http://localhost:4000`.

The `remote_theme` in `_config.yml` is what GitHub Pages uses.  Local builds
may need `bundle add webrick` on Ruby 3+.
