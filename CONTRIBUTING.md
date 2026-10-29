# Contributing to Hazel

Start with [getting started](docs/getting-started.md), the [documentation index](docs/README.md) and [development/testing](docs/development.md). Current cleanup work targets `feature/runtime-ui-hardening`; master integration follows final qualification and acceptance.

## Report a useful issue

Use [GitHub Issues](https://github.com/cringlekaden/Hazel/issues) for reproducible bugs or documentation corrections. Include:

- Commit SHA or package `build.json`, application, build configuration and exact command.
- OS/version, GPU/driver/OpenGL version, display path (X11/Xwayland on Linux) and relevant DPI/controller details.
- Small reproduction, expected/actual result, and whether it occurs in Edit, Play or a relocated package.
- Console/compiler log excerpt and a minimal licensed project when needed. Remove tokens, usernames/private paths and personal saves before sharing.

Distinguish automated/software checks from physical hardware acceptance. Preserve the original project and player data when reproducing a failure. There is no promised response time or commercial support service for this fork.

## Propose a change

Keep the change scoped and explain its behavior and meaningful verification. Use Premake and the public Python CLI, preserve Linux/Windows implementations, OpenGL 4.1, Windows UTF-8/dynamic CRT and existing ownership conventions. Keep pinned dependencies and vendor trees unchanged unless the task explicitly requires a reviewed dependency change.

Update current guides alongside code/tool/workflow changes. Put dated investigation evidence in the historical records, rather than adding checkpoint diaries to the landing page. Run focused existing checks first; complete relevant milestone gates once before submission. Do not weaken assertions or substitute screenshots for correctness tests.

Submit a pull request with the problem, resulting behavior, tests run and any outstanding platform/manual checks. CI runs on PRs; topic-branch maintainers can dispatch the canonical workflow explicitly. Do not clean another developer's ignored files, rewrite example drafts, publish a release or change repository protection settings as incidental cleanup.

Retain [upstream attribution and third-party notices](docs/platforms-and-dependencies.md#licensing-and-provenance). Screenshots must come from the implemented application and use disposable projects/profiles; capture notes live in [docs/images](docs/images/README.md).
