# AGENTS.md

Arduino firmware for the ATS Mini ESP32-S3/SI4732 receiver.

- Firmware lives in `ats-mini/`; Sphinx documentation in `docs/source/`.
- Read `CONTRIBUTING.md` and `docs/source/development.md` before making changes.
- For receiver screenshots, follow [the screenshot skill](.agents/skills/ats-mini-screenshot/SKILL.md).
- Follow surrounding C++ style; avoid unrelated reformatting.
- Internal DRAM is scarce; PSRAM is plentiful. Balance memory savings with performance, safety, and simplicity.
- Build firmware changes from the repository root:
  `arduino-cli compile --warnings all ats-mini`
- Run repository checks: `uv run prek run --all-files`.
- Keep board settings in `ats-mini/sketch.yaml` and
  `.github/workflows/build.yml` consistent when changing them.
- For user-visible firmware changes, edit an existing unreleased Towncrier fragment for the same feature in `changelog/`, or create one if none exists. Skip documentation changes, dependency bumps, and internal changes with no user-visible effect. See `pyproject.toml` for categories.
- Update documentation when user-facing behavior changes. Stay brief, use the existing writing style.
- Do not compile anything unless asked.
- Report validation performed and any behavior needing hardware testing.
