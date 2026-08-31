---
layout: default
title: "C++ Modernization Notes"
nav_order: 56
permalink: /cpp-modernization
---

# C++ Modernization Notes (Beyond the C++11 Baseline)

Needful-enhanced targets C++11 by design. This page records what newer
standards would buy if that baseline ever changes.

## Why This Is Separate From Design Rationale

`docs/design.md` explains why C++11 remains the floor. This page is a
catalog of optional improvements, so maintainers can evaluate tradeoffs
without re-deriving them.

## C++14: Small Cleanup, Limited Payoff

C++14 would help readability more than capability.

- `std::enable_if_t` and `std::remove_pointer_t` remove repetitive `typename`
  boilerplate in traits and constructor SFINAE.
- Generalized `constexpr` allows more helper functions to be expressed as
  functions instead of enum-style constants.
- Generic lambdas are occasionally useful in test helpers.

Impact on `needful-contra.hpp` is modest: fewer template aliases and less
ceremony, but no major semantic simplification.

## C++17: Largest Practical Gain

C++17 is where the file can become meaningfully smaller and more robust.

- `if constexpr` can replace several trait-specialization branches with one
  primary template plus local compile-time branches.
- `std::void_t` and cleaner detection idioms reduce custom wrapper-probing
  plumbing.
- Inline variables can simplify trait constants and avoid out-of-class
  definitions when needed.
- `[[nodiscard]]` already matters for `Fallible(T)` diagnostics and negative
  tests; this is a concrete quality gain, not just syntax.

For `SinkWrapper` and `InitWrapper`, C++17 also makes it easier to collapse
constructor overload sets while keeping diagnostics clear.

## C++20: Major Expressiveness Upgrade

C++20 would allow the strongest simplification of all trait-gated interfaces.

- Concepts can replace SFINAE-heavy constructor/assignment constraints with
  direct requirements (`requires Contravariant<U, T>`), producing better
  errors and less metaprogramming scaffolding.
- `consteval` and improved `constexpr` can push more invariants into immediate
  compile-time checks.
- `[[no_unique_address]]` can reduce wrapper footprint in some cases (where
  empty policy fields are introduced later).

A concept-based rewrite would be the first version where `needful-contra.hpp`
reads primarily as policy code, not metaprogramming mechanics.

## Contra-Specific Hotspots To Revisit If The Floor Moves

If Needful adopts C++17+ in the future, these are the highest-value targets:

1. Merge `IsContravariantLayout` and `IsContravariant` logic into a smaller
   trait surface with local `if constexpr` checks.
2. Revisit `SinkWrapper` constructor/assignment duplication: keep transfer
   semantics, but reduce overload count and improve self-documentation.
3. Keep the mutable pending-corruption model explicit even after refactors:
   this is semantic state transfer, not incidental mutability.
4. Evaluate replacing ad hoc wrapper detection with a standardized detection
   idiom across all enhanced headers.

## Non-Goals Even Under Newer Standards

Some complexity is intentional and should remain:

- Contravariant direction checks tied to same-layout inheritance.
- Deferred corruption semantics in `Sink(T)`.
- Distinct semantics between `Contra(T)`, `Sink(T)`, and `Init(T)`.
- Interoperability with plain-C expansion rules in `needful.h`.

Modernization should remove boilerplate, not erase these contracts.
