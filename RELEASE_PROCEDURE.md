# Release procedure

- Update [version_history.md](docs/root/version_history.md). 
  - Make sure breaking changes are explicitly called out.
  - Ensure the release notes are complete.
- Tag the release in git using the current version number:
```bash
git tag -a "v0.3" -m "Release v0.3"
git push origin v0.3
```
- Bump `MAJOR_VERSION` / `MINOR_VERSION` in [version.h](include/nighthawk/common/version.h) to the next version.
