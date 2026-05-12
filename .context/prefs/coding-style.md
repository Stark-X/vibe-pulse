# Coding Style Guide

> 此文件定义团队编码规范，所有 LLM 工具在修改代码时必须遵守。
> 提交到 Git，团队共享。

## General
- Prefer small, reviewable changes; avoid unrelated refactors.
- Keep functions short (<50 lines); avoid deep nesting (≤3 levels).
- Name things explicitly; no single-letter variables except loop counters.
- Handle errors explicitly; never swallow errors silently.

## Rust
- Use `Option`/`Result` idioms; avoid `unwrap()` in production paths.
- Prefer `?` operator for error propagation.
- Zero warnings policy: `cargo clippy -- -D warnings` must pass.
- No unused imports or dead code in non-test modules.

## JavaScript (Vanilla)
- No framework — vanilla JS only, use `h()` hyperscript helper.
- Static imports at top; no dynamic `import()` in hot paths.
- Hash-guard all re-renders: compare `JSON.stringify` before calling `render()`.
- No `innerHTML` with user-derived data (XSS prevention).

## Git Commits
- Conventional Commits with emoji prefix, imperative mood.
- Atomic commits: one logical change per commit.
- Subject line ≤ 72 chars.

## Security
- Never log secrets (tokens/keys/cookies/JWT/API keys).
- Validate inputs at trust boundaries.
- CSP must be explicit — no `unsafe-eval`, minimize `unsafe-inline`.
