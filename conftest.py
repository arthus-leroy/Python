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

# catch the output of a passed test and include it in the html report
def catch_log(dict):
    def pytest_html_results_table_html(report, data):
        if report.passed and dict:
            res = html.div(class_ = "extra")

            for key in dict:
                    res.append(html.tr(
                        key + html.span("::after", class_ = "expander")))
                    res.append(html.div(
                        html.span(dict[key], class_ = "log"), class_ = "colapsed"))

            data = [res]