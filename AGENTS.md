## graphify

This project has a knowledge graph at graphify-out/ with god nodes, community structure, and cross-file relationships.

Rules:
- For codebase questions, first run `graphify query "<question>"` when graphify-out/graph.json exists. Use `graphify path "<A>" "<B>"` for relationships and `graphify explain "<concept>"` for focused concepts. These return a scoped subgraph, usually much smaller than GRAPH_REPORT.md or raw grep output.
- If graphify-out/wiki/index.md exists, use it for broad navigation instead of raw source browsing.
- Read graphify-out/GRAPH_REPORT.md only for broad architecture review or when query/path/explain do not surface enough context.
- After modifying code, run `graphify update .` to keep the graph current (AST-only, no API cost).

## Zen build version

- Every completed change set must increment the Zen firmware build number in `examples/companion_radio/zen-overlay/app/MyMesh.h` using the format `v1.32.x` (for example, `v1.32.1`, then `v1.32.2`). Increment once per requested update, including documentation or build-configuration changes, before validation and handoff. Read-only reviews and builds with no repository changes do not increment it.
- Keep the current release shown in `README.md` identical to the firmware version.

## Zen build artifacts

- Use only the PlatformIO environments `WioTrackerL1_Zen_OLED` and
  `WioTrackerL1_Zen_E-INK` for Zen firmware.
- Write versioned UF2 files directly inside their PlatformIO build directory as
  `<environment>.<version>.uf2`, with no leading `v` in `<version>`.
- Keep local, scripted and GitHub release builds on this same naming convention.
- After every completed repository change, compile and verify both the OLED and
  E-ink Zen targets before handoff. Never validate only one display target.
- Expected paths are `.pio/build/WioTrackerL1_Zen_OLED/WioTrackerL1_Zen_OLED.<version>.uf2`
  and `.pio/build/WioTrackerL1_Zen_E-INK/WioTrackerL1_Zen_E-INK.<version>.uf2`.
