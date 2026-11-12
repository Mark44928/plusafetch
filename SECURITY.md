# Security policy

Plusafetch runs entirely offline, performs no network calls (there is no internet fetch anywhere in this codebase), and stores no credentials.

To report a vulnerability (e.g., a readable path escalation, a config-parse issue, an unsafe temp-file pattern):

- Email the maintainers via the repository contacts, or open a private GitHub Security Advisory on the repository.
- Please include: platform (Termux version / distro), steps to reproduce, and impact.
- Do not disclose publicly until a fix is available.

Scope: this repository only. Third-party deps: `nlohmann/json` (see THIRD_PARTY_NOTICES.md).
