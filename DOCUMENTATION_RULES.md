# Documentation Rules

These rules apply to documentation in both Gazeora repositories:

- Desktop/core: https://github.com/Gazeora/Gazeora
- Android companion: https://github.com/Gazeora/Gazeora-Android

This file defines the shared documentation standard. Each repository's `CLAUDE.md` adds project-specific requirements. If a project-specific instruction is stricter, follow it. If instructions conflict or the correct behavior is unclear, ask before making a potentially breaking documentation or implementation decision.

---

## 1. Purpose and source of truth

Documentation must help users build, install, configure, operate, troubleshoot, and understand the software, and help contributors safely modify it.

- The current implementation is the source of truth: source code, tests, build files, manifests, resources, schemas, and shipped assets.
- Inspect the relevant implementation before writing or changing a claim.
- Never document a planned feature as available. Label planned, experimental, incomplete, or unsupported behavior explicitly and only when that status is known.
- Never invent commands, configuration options, environment variables, protocol fields, compatibility, platform support, dependencies, performance numbers, security properties, or guarantees.
- If evidence is missing, omit the claim or state that it has not been verified.
- Keep documentation consistent across desktop and Android repositories. For shared behavior, designate one canonical description and link to it from the other repository where practical.

## 2. Documentation structure

Prefer a small, navigable documentation set over one oversized README. Adapt to the repository's existing structure before creating new directories.

Recommended structure (use only what the project needs):

```text
README.md
DOCUMENTATION_RULES.md
docs/
  getting-started.md
  user-guide.md
  architecture.md
  configuration.md
  troubleshooting.md
  development.md
  testing.md
  protocol.md             # only if a real cross-app protocol exists
  decisions/              # significant, accepted architecture decisions
CHANGELOG.md              # if the project maintains a changelog
CONTRIBUTING.md           # if external contributions are supported
SECURITY.md               # if a security reporting process is defined
```

Rules:
- Do not create empty placeholder documents just to match this tree.
- Respect existing filenames, conventions, and navigation.
- Keep one canonical home for each topic. Link to it rather than copying large sections.
- Keep repository-specific instructions in that repository; keep genuinely shared rules here.
- Avoid duplicated README content across the desktop and Android repositories.

## 3. README requirements

The root `README.md` is the primary entry point. It should be concise and answer:

1. What is this project?
2. What does it currently do?
3. What is required to use or build it?
4. How does a user or developer get started?
5. Where are detailed guides, architecture notes, troubleshooting steps, and contribution instructions?

Include only sections relevant to the repository, such as:
- Project name and a short, accurate description.
- Current status and supported capabilities, with limitations clearly stated.
- Screenshots or diagrams only when current, useful, and maintained.
- Prerequisites and verified installation/build/run steps.
- A minimal quick-start example.
- Links to user, developer, architecture, troubleshooting, and contribution docs.
- License information that matches the actual repository license files.

Do not fill the README with speculative roadmaps, unsupported badges, stale screenshots, marketing claims, or exhaustive implementation details.

## 4. Writing style and Markdown

- Use clear, direct, professional language. Prefer short sentences and active voice.
- Define acronyms on first use when the intended audience may not know them.
- Use consistent names for features, modules, settings, message fields, and UI labels.
- Use headings in a logical hierarchy; do not skip heading levels without reason.
- Use fenced code blocks with language identifiers (`bash`, `powershell`, `kotlin`, `cpp`, `json`, etc.).
- Use inline code for filenames, paths, commands, identifiers, settings, and literal values.
- Use tables for compact comparisons or reference data, not for long prose.
- Use relative links for files within the same repository. Check paths and heading anchors.
- Use descriptive link text rather than “click here”.
- Keep lines and paragraphs readable. Avoid unnecessary decoration, emoji, and oversized headings.
- Use consistent spelling, capitalization, units, and terminology.

## 5. User documentation

User-facing docs should describe observable behavior, not internal assumptions.

When applicable, explain:
- Prerequisites and supported environment (only if verified).
- Installation and first-run steps.
- Pairing, connection, setup, calibration, and normal operating workflows.
- Settings and their defaults, valid ranges, units, and effects.
- Permission prompts and the consequences of granting or denying permissions.
- Expected status indicators and common error states.
- Known limitations and safe recovery steps.

Keep instructions synchronized with the actual UI and shipped behavior. Do not tell users to select a menu item, toggle, button, or setting that does not exist in the current version.

## 6. Developer and build documentation

Build and development instructions must be reproducible from a clean checkout as far as the repository supports.

For each documented workflow:
- State the required tools and versions only when verified from project configuration or tested environments.
- Show the correct working directory.
- Provide commands exactly as tested, including platform-specific syntax where needed.
- Explain required environment variables or local configuration without exposing secret values.
- Distinguish required steps from optional steps.
- Explain expected output or how to confirm success.
- Include relevant test, lint, formatting, packaging, and clean commands only when those tasks exist.

Never claim that a build or test passed unless it was run successfully in the current task or supported by reliable existing CI evidence. If a command cannot be tested, label it unverified and explain why.

## 7. Architecture and technical documentation

Architecture docs should explain the current system, not the architecture someone hopes to build.

When relevant, cover:
- Major components and each component's responsibility.
- Module boundaries and dependency direction.
- Important runtime flows and lifecycle boundaries.
- Data ownership and transformations.
- Threading, concurrency, synchronization, and resource ownership.
- External dependencies and integration boundaries.
- Error handling and recovery paths.
- Important constraints and trade-offs.

Use diagrams only when they clarify real relationships or flows. Keep diagram source editable (for example Mermaid) and update it when the implementation changes. Avoid diagrams that imply components or connections that do not exist.

## 8. API, configuration, and protocol documentation

For every documented API, setting, command, or protocol field, verify its name and behavior against the implementation.

Where applicable, document:
- Exact name and purpose.
- Type, format, units, allowed values, and defaults.
- Required/optional status.
- Validation and error behavior.
- Compatibility and versioning behavior.
- A minimal valid example.

For communication between repositories, make message schemas and shared terminology consistent. Document byte order, coordinate frames, units, timestamps, version negotiation, and error semantics only where they are defined by the implementation. Do not claim authentication, encryption, privacy, or delivery guarantees unless verified in code and configuration.

## 9. Troubleshooting

Troubleshooting entries should be actionable and evidence-based. For each issue, include as many of these as are known:

- Observable symptom or exact error text.
- Likely cause(s), distinguished from confirmed causes.
- Steps to diagnose safely.
- A fix or workaround that is supported by the implementation.
- How to verify recovery.
- Relevant platform/version constraints.

Do not recommend destructive cleanup, data deletion, disabling security protections, or changing system settings without explaining the risk and providing a safer alternative where possible.

## 10. Changelog, contribution, and security docs

- Changelogs, when maintained, should describe user- or developer-relevant changes and use consistent release/version references.
- Contribution instructions must match the actual branch, build, test, review, and commit workflow. Do not invent a process.
- Security documentation must provide a real, monitored reporting path. Never publish secrets, exploit details that create unnecessary risk, or private vulnerability reports.
- License statements must match the checked-in license and any applicable third-party notices. Do not infer licensing terms from repository names or README wording.

## 11. Screenshots, diagrams, and examples

- Use current screenshots from the actual application. Remove personal data, tokens, device identifiers, private paths, and unrelated desktop content.
- Give images meaningful alt text and keep image files in a predictable repository location.
- Prefer diagrams that can be maintained as source text (such as Mermaid) over manually edited images when practical.
- Examples must be minimal, syntactically valid, and consistent with the current implementation.
- Clearly label pseudocode, illustrative examples, mock data, and unverified commands.
- Do not include real credentials, private keys, access tokens, private server addresses, or production user data.

## 12. Keeping documentation synchronized

Update docs in the same change when a change affects:
- User-visible behavior or workflows.
- Installation, build, packaging, or runtime requirements.
- Supported platforms, devices, graphics APIs, sensors, or integrations.
- Settings, defaults, environment variables, command-line options, or configuration files.
- Public interfaces, data formats, schemas, or inter-process/network communication.
- Architecture, module boundaries, dependencies, or developer workflows.
- Known limitations, security behavior, or troubleshooting steps.

Do not make unrelated documentation rewrites during a focused code change. Keep the diff scoped and preserve accurate existing content.

## 13. Documentation review checklist

Before completing documentation work, verify:

- [ ] Every factual claim is supported by the current implementation or clearly qualified.
- [ ] No planned or experimental capability is presented as generally available.
- [ ] README describes the current project and links to the right detailed docs.
- [ ] Commands, paths, filenames, settings, and UI labels are correct.
- [ ] Code examples and configuration snippets are valid and current.
- [ ] Internal links and heading anchors resolve.
- [ ] Cross-repository descriptions and protocol details agree.
- [ ] Screenshots and diagrams are current and contain no sensitive information.
- [ ] License, security, compatibility, and performance claims are verified.
- [ ] The change does not duplicate or contradict existing documentation.
- [ ] Relevant Markdown, link, build, or documentation checks were run where available.
- [ ] The final summary states which docs changed and which validations were actually performed.

## 14. Claude Code task behavior

When working in either repository:

1. Read this file and the repository's `CLAUDE.md` before documentation changes.
2. Inspect the existing docs and the implementation affected by the task.
3. Identify which documentation is affected before editing.
4. Make the smallest complete documentation update that accurately describes the change.
5. Validate links, examples, and commands where practical.
6. Summarize the changed files and validation results. Mention anything that could not be verified.

Do not create a second documentation policy file or silently override repository-specific rules. If a required detail cannot be established from the repository, ask or explicitly leave it undocumented.
