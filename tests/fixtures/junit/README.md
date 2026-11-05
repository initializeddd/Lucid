These small regression fixtures were generated from actual local test runs:

- `pytest.xml` and `pytest-legacy.xml`: pytest 8.4.2, xunit2 and legacy families.
- `pytest-call-teardown.xml`: pytest 8.4.2 reports a failing call and teardown as two rows for one test identity.
- `jest-junit.xml`: Jest 29.7.0 with jest-junit 16.0.0.
- `junit-platform.xml`: JUnit Platform Console 1.11.4, Jupiter engine.

The main fixtures contain one passing, one failing, and one skipped test. Hostnames,
timestamps and temporary paths were sanitized. Java environment properties were
removed; captured output and stack traces were shortened to avoid local paths.
The fixture structure and status elements are preserved.
