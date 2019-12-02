import pytest

# rewrite of a hook to remove the . (for PASSED) and F (for FAILED)
def pytest_report_teststatus(report):
    category = ''

    if hasattr(report, "wasxfail"):
        if report.skipped:
            category = "xfailed"
        elif report.passed:
            category = "xpassed"
    elif report.when in ("setup", "teardown"):
        if report.failed:
            category = "error"
        elif report.skipped:
            category = "skipped"
    else:
        category = report.outcome

    return (category, "", "")