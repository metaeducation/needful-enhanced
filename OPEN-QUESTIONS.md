# Open Questions

Things that are **not settled**. This file exists so a position can be recorded
without being enacted.

That distinction is the whole point. Without a place like this, the only way to
register "I think the tagline is wrong" is to *change the tagline* -- so
disagreements arrive as commits, and the reasoning evaporates. Here, a case can
be made and left to sit.

Nothing in this file is binding. A "leaning" is a leaning.

### How entries work

Each entry states the question, the options with their **honest costs**, and a
current leaning.

### The anti-rot rule

The failure mode for a file like this is not being wrong. It is **accumulating**
-- growing into a graveyard of stale musings that nobody reads, which is worse
than not having one, because it looks like documentation.

So there is exactly one rule: **an entry either moves or dies. It does not
sit.**

Deciding an entry means *relocating its reasoning*, never deleting it and never
leaving it here marked "resolved":

| Outcome | Where the reasoning goes | Why there |
|---|---|---|
| Decided, and it constrains future edits to the code | [`docs/design.md`](docs/design.md) | That file is read before editing `needful.h` |
| Decided, and users will ask about it | [`docs/faq.md`](docs/faq.md) | Published; it is an answer, not a constraint |
| Decided, and it is a copy or positioning choice | the artifact itself, plus a line in `## Closed` below | There is nowhere else it belongs |
| Abandoned | delete it outright | An idea nobody acted on is not history |

**The `## Closed` section at the bottom is a staging area, not an archive.** An
entry lands there only long enough to record the outcome and point at wherever
the reasoning went. Once that relocation is done, the stub gets deleted too.
If `## Closed` is growing, the relocation step is being skipped.

**Expiry:** if an entry has sat untouched through two rounds of real work on the
thing it concerns, it is not actually an open question -- it is a preference
nobody holds strongly. Decide it or cut it.

---

## Marketing and positioning

### Should this file be public?

Committing design rationale is common (ADRs, `RATIONALE.md`). Committing
*marketing* uncertainty is rare, and the rarity might itself be the interesting
part -- it's the same move as the "Weak Points" section on the homepage, one
level up: a project that can say "here's what we haven't figured out" reads as
more confident than one projecting total certainty.

- **Repo-only (current).** Lives at the repo root, outside `docs/`, so Jekyll
  never publishes it. Contributors see it; evaluators don't. Safe.

- **Published.** Becomes part of the pitch -- "we argue about this in the open."
  Risk: an evaluator skimming for reasons to say no finds a list of things the
  author isn't sure about, and reads it as instability rather than candor.

*Leaning:* repo-only until there is a reason to publish. It costs nothing to
promote later; it's awkward to retract.

### Is the honest tagline worth the weaker tagline?

Live example, and the reason this file exists.

- `Rust-grade safety for C` -- punchy, and an overclaim. "Safety" in the C/Rust
  conversation means *memory* safety. Needful does not deliver that, and the
  claim is checkable in under a minute.

- `Rust-style types for C` -- accurate and limp. "Types" has no stakes in it.

- `Rust-grade type checking for C` -- **current.** Keeps the phrase that carried
  the energy; the noun it modifies is now something actually delivered.

*Leaning:* current wording. But the underlying question generalizes and is not
settled: **how much punch is accuracy worth?** The [Weak Points](docs/index.md)
section bets heavily that candor *is* the marketing. If that bet is right, the
tagline should be the most conservative claim on the site, not the loosest.

### Should the non-memory-safety caveat appear above the fold?

Needful is explicit that it is not a memory-safety system, but where that
appears changes first impressions.

- **Lead with it near the top.** Increases trust with skeptical evaluators and
  security reviewers; reduces risk of an "overclaim" read.

- **Keep it in Weak Points only.** Keeps the opening punchier and shorter;
  risks some readers deciding before they reach the caveat.

*Leaning:* lead with it briefly, then expand under Weak Points.

### Should warnings-as-errors be framed as a core requirement earlier?

Many Needful checks surface as warnings first, so enforcement depends on
`-Werror` / `/WX` in the checked build.

- **Mention in Getting Started.** Makes the operational contract explicit at
  adoption time; fewer "it compiled, so I guess we're safe" misreads.

- **Keep only in Weak Points and test docs.** Less upfront friction in the
  pitch; higher risk that teams miss the enforcement dependency.

*Leaning:* mention once in Getting Started, with details in Weak Points/docs.

### Should docs-as-tests get its own documentation page?

The guarantee and the mechanics are different audiences, and they have already
been split: the *guarantee* ("every example here is compiled in CI, the
negative ones must fail for the stated reason") is a Design Goal on the
homepage, because it changes how much a reader should trust the samples. The
*mechanics* (fence tagging, `MATCH-ERROR-TEXT`, the extractor) are in
`docs/README.md`, because they only matter to someone adding a test.

So the open part is narrower: is there a third thing, aimed outward?

- **No page.** Current. Nothing is missing; a page about our own test
  infrastructure risks reading as navel-gazing on a site meant to sell a
  library.

- **A page, pitched as reusable methodology** -- "how to make documentation
  that cannot rot," written so another project could copy the approach. That
  is a genuinely different artifact from documenting *our* setup, and it is
  the version with an audience beyond us.

*Leaning:* no page for now. If one gets written it should be the methodology
version, which makes it a candidate opener for outward-facing material -- see
the introduction question above, and note the two would then compete.

### Does the name carry its own weight?

`Needful` is memorable but opaque -- it tells a stranger nothing, and `Need(T)`,
the namesake, is the construct with the least immediate appeal to someone who
hasn't seen it in use.

- **Keep, always paired with the tagline.** The tagline does the explaining, so
  the name never appears naked. Current de-facto practice.

- **Rename.** Enormous cost (URLs, repos, the corpus of prose) for a benefit
  that is real but unmeasured.

*Leaning:* keep, and treat "name never appears without tagline" as a rule
rather than a habit.

### What should an outward-facing introduction lead with?

Scoped to *outward* material -- a talk, a README for a new audience, a post if
one ever gets written. **This does not reopen the banner example**, which is
settled; see the note under Closed.

- **`Option`/`Result` for C.** Accurate; also a crowded, ignored genre where
  the reader has a pre-formed "another one of these" response ready.

- **The comment macros** -- *"Comments that fail the build."* The most novel
  idea here, the cheapest to demo, and repeatedly the thing fresh readers
  single out unprompted. Not the biggest feature, and it invites "cute" as a
  first reaction.

- **The asymmetry** -- *"Ship the same binary. Check it harder in CI."* The
  strongest argument on the merits, but it needs a paragraph to land rather
  than a snippet, which makes it a bad opener and a good closer.

*Leaning:* none held firmly. The tension is real -- the most *novel* thing and
the most *persuasive* thing are different things, and leading with novelty
risks the whole library being filed under "clever."

---

## Design

### Should there be a generated single-file amalgamation?

The two-repository split is listed as a weak point on the homepage, and it is
the moment the "one header, no dependencies" story stops being simple.

SQLite's amalgamation is the obvious precedent: a build step that inlines
`needful-enhanced/*.hpp` into one distributable header.

- **Cost:** a real build step to maintain, a second artifact that can drift, and
  the loss of the property that a reader can see exactly which file does what.

- **Benefit:** deletes an entire objection from the adoption path.

*Leaning:* worth prototyping, not worth committing to sight-unseen. The
question to answer first is whether the amalgamated file is still *auditable* --
if it isn't, it defeats the reason the C definitions are written out in full.

### Should the prefix keywords have a parenthesized alternative spelling?

`unwrap foo` costs precedence surprises and permanent low-grade tooling
friction (formatters, highlighters, code search). The mitigation is strong -- a
misparse is always a compile error, never a wrong answer -- but it is the
feature most likely to read as "too clever" to a newcomer.

- **Offer `unwrap_(foo)` alongside** so a project can adopt Needful without
  adopting the trick.

- **Don't.** Two spellings for one operation is its own tax, and codebases
  would end up mixing them.

*Leaning:* don't, unless someone actually declines to adopt over it. This is a
question to be answered by a real user, not by argument.

### Does `known(T, expr)` belong in the headline feature list?

It is a macro-author's tool, not an application programmer's. Listing it
alongside `Option` and `Result` slightly misrepresents who the library is for.

- **Keep it listed**, since it is genuinely useful and someone writing macros
  needs to discover it.

- **Move it to a second tier** with the shims.

*Leaning:* second tier, but the header's list is short enough that this has not
been worth churn.

### What is the thread-safety story for `Result(T)`?

The default hooks are single-threaded and the design assumes one pending
failure at a time. Today this is documented and left to the user.

- **Leave it.** The hooks are the extension point; that is what they are for.

- **Ship a thread-local default** so the obvious first thing a user tries is
  also correct in a threaded program.

*Leaning:* unresolved, and the most likely of these to become a real bug report.

---

## Closed

*(Move entries here briefly when decided, with the outcome, then relocate the
reasoning per the table above and delete the stub.)*

- **Which construct gets the banner example in `needful.h`?** -- `Option(T)`.
  The opening example must also teach the C-versus-enhanced mechanism, so it
  can only afford one unfamiliar idea, and the mechanism is the one that
  matters. Recorded in [`docs/design.md`](docs/design.md).

- **Should `Need(T)` lead the feature list, given `Option(T)` is more broadly
  useful?** -- Yes, it leads. `Option(T)` is defined *in contrast to* a value
  that cannot be absent, so Need establishes the axis; and the banner example
  already uses `Option(T)`, so the more-useful construct does the persuading
  before the list starts. Recorded in [`docs/design.md`](docs/design.md).
