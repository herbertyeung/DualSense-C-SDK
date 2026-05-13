# Progress

## 2026-05-13

- Created a concrete 0.3 goal: maintainability and runtime polish.
- Chose a conservative scope: no new feature expansion, focus on refactor, necessary comments, and algorithm optimization.
- Recorded current project state, maintenance hotspots, and likely optimization candidates.
- Recommended starting order: baseline verification, test refactor, then protocol comments/constants.
- Started Phase 1.
- Built `build-vs2026` Debug successfully.
- Ran CTest successfully: 1/1 `dualsense_tests` passed.
- Installed the SDK to `build-vs2026\install-sdk`.
- Verified the external CMake consumer after cleaning duplicate `Path`/`PATH` from the child process environment and using an absolute `CMAKE_PREFIX_PATH`.
- Ran `dualsense_diag`; one USB DualSense is connected with `capabilities=0x1fff`, and both speaker and microphone audio endpoints are visible.
- Marked Phase 1 complete and started Phase 2.
- Added a named-test harness to `tests/test_main.cpp`.
- Added CTest labels to `dualsense_tests`.
- Built `dualsense_tests` and ran CTest successfully.
- Ran `dualsense_tests.exe` directly and confirmed it prints one `PASS` line per named test.
- Marked Phase 2 complete and started Phase 3.
- Named report offsets and added protocol comments in `src/output_report.cpp` and `src/report_parser.cpp`.
- Built `dualsense_tests` and ran CTest successfully after protocol cleanup.
- Extracted shared PCM16 WAV parsing into `tools/wav_pcm.cpp` and `tools/wav_pcm.h`.
- Updated diagnostic, ship demo, and test targets to use the shared WAV parser.
- Added a WAV parser test against `assets/commander_ready.wav`.
- Optimized target-lock filtering to use squared distance and cosine threshold before score calculation.
- Added a target-lock regression test for close-but-outside-angle targets.
- Ran full Debug build successfully.
- Marked Phase 3 and Phase 4 complete; Phase 5 remains in progress.
- Reused ship-demo scratch vertex vectors for feedback zones, engine flame, and light bullets.
- Refactored gyro stillness calibration helper methods without changing behavior.
- Built `dualsense_tests` and `dualsense_ship_demo`, then ran CTest successfully.
- Marked Phase 5 complete and started Phase 6.
- Added `docs/known-limitations.md` and `docs/release-checklist.md`.
- Updated README docs links, changelog, and productization status.
- Ran full Debug build successfully.
- Ran CTest successfully.
- Ran install successfully and confirmed new docs are installed.
- Ran `dualsense_diag` successfully; USB controller and both audio endpoints are visible.
- Marked Phase 6 complete.
