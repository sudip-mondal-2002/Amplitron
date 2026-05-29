# Amplitron Pull Request Template

## Description
<!-- Provide a clear description of the problem solved and the approach taken. -->

## Related Issue
<!-- Refer to the issue this PR resolves. Must use one of the standard GitHub keywords to auto-close: Closes #XX, Fixes #XX, Resolves #XX. -->
Closes #

## Type of Change
- [ ] 🐛 **Bug Fix** (non-breaking change which fixes an issue)
- [ ] ✨ **New Feature** (non-breaking change which adds functionality)
- [ ] ♻️ **Refactor / Cleanup** (structural improvement, no user-visible behavior change)
- [ ] ⚡ **Performance Optimization** (optimizes CPU/memory efficiency in DSP or GUI)
- [ ] 📝 **Documentation** (updates or additions to docs/guides)
- [ ] ✅ **Testing** (adding or refactoring tests)
- [ ] 🔧 **CI/CD / Configuration** (updates to workflows or build configuration)

## Verification & Testing
### Automated Tests
- [ ] All unit and integration tests pass successfully (`./amplitron-tests` or corresponding suite)
- [ ] Added new tests verifying the exact fix/feature (if applicable)

### Manual Verification
<!-- Describe the manual validation steps performed to ensure correctness (e.g. CLI tests, audio interface checks). -->

## real-time Safety & Concurrency Check
- [ ] **No blocking calls on Audio Thread**: Verified that the DSP/process hot paths do not allocate memory, log to stdout, or block on standard mutexes.
- [ ] **Thread boundaries respected**: State transfer between GUI and DSP uses lock-free spsc queues or shadow states.

## Checklist
- [ ] Code follows Amplitron's naming conventions and C++17 coding standards.
- [ ] Changes do not introduce compiler/linker warnings.
- [ ] No unrelated modifications or styling-only edits are included.
