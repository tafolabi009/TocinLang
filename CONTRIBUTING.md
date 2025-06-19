# Contributing to Tocin

Thank you for your interest in contributing to Tocin! We welcome contributions of all kinds—code, documentation, tests, and ideas.

## How to Contribute
1. **Fork the repository** and create a new branch for your feature or fix.
2. **Write clear, well-documented code** following the style guide below.
3. **Add or expand tests** for any new features or bug fixes.
4. **Update documentation** as needed (README, docs/, code comments).
5. **Open a Pull Request** with a clear description of your changes.

## Code Style
- Use consistent indentation (4 spaces for C++, 2 spaces for Tocin code).
- Use descriptive variable and function names.
- Document all public APIs and complex logic with comments or docstrings.
- Prefer modern C++ (C++17+) idioms and best practices.
- Write idiomatic, readable Tocin code in examples and tests.

## Adding Tests
- Add `.to` files to the `tests/` directory for new features or bug fixes.
- Cover both positive and negative cases (use comments for expected errors).
- Run all tests before submitting your PR.

## Documentation
- Update `README.md` and relevant files in `docs/` for new features.
- Add code examples and troubleshooting tips where helpful.

## Pull Request Process
- Ensure your branch is up to date with `main`.
- Write a clear, descriptive PR title and summary.
- Reference related issues or discussions if applicable.
- Be responsive to code review feedback.

## Community Guidelines
- Be respectful and constructive in all discussions.
- Help others by reviewing PRs and answering questions.
- Report bugs and suggest features via GitHub Issues.

## Continuous Integration (CI/CD)

- All pull requests and pushes are automatically built and tested using GitHub Actions.
- The workflow runs on Windows, Linux, and macOS using a build matrix.
- All tests in `tests/` must pass for a PR to be merged.
- Code coverage is collected and reported for each build.
- Contributors can check CI status on the PR page or in the Actions tab.
- If CI fails, review the logs, fix issues, and push updates until all checks pass.

Thank you for making Tocin better! 