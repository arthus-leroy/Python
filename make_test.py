#!/bin/python

from os import listdir, isatty
from os.path import splitext, join, getmtime
from subprocess import Popen, PIPE, check_output, run
from sys import stderr as err

from ansi2html import Ansi2HTMLConverter as Converter
from html import unescape
from bs4 import BeautifulSoup

import pytest_html

TEST_DIR = "tests"
PKG = check_output(("pkg-config", "--cflags", "--libs", "python3")).decode().split(' ')[:-1]
MAKE_CMD = ("g++", "-g3", "-Wall", "-Wextra", "-Werror", "-pedantic", "-std=c++17", \
            "-I.", "-DTRACEBACK", "-fdiagnostics-color=always", *PKG)
#MAKE_CMD += ("-DPYDEBUG_CONST",)

# files used in every test and that need to be compiled and liked statically
invariant_files = ("python.hh", "backtrace.hh", "backtrace.cc")
invariant_time = max([getmtime(file) for file in invariant_files])

# convert .cc -> .o with error check
def make_o(cc: str, dir: str = ""):
    if dir:
        cc = join(dir, cc)

    name = "%s.o" %splitext(cc)[0]

    # just compile if doesn't exist
    try:
        t = getmtime(cc)

        # if file not changed, don't compile back
        if t >= invariant_time and t >= getmtime(cc):
            return name
    except:
        pass

    cmd = MAKE_CMD + ("-c", "-o", name, cc)

    p = Popen(cmd, stderr = PIPE)
    stderr = p.communicate()[1]

    if p.returncode != 0:
        print(stderr.decode(), file = err)
        exit(1)

    if dir:
        return join(dir, name)

    return name

# source files in the invariant files
o_files = ("backtrace.cc",)
o_compiled = [make_o(file) for file in o_files]

header = """
from os import isatty
from os.path import splitext, join, getmtime
from subprocess import Popen, PIPE
from sys import stdout as out, stderr as err
from pytest import fail
from logging import info, INFO

# return the content in green bold if fd is a tty
def green(content: str, fd: int):
    if isatty(fd):
        return "\033[38;5;35;1m%s\033[0m" %content

    return content

# return the content in red bold if fd is a tty
def red(content: str, fd: int):
    if isatty(fd):
        return "\033[38;5;196;1m%s\033[0m" %content

    return content

# report a failed test
def fail_test(name: str, content: str):
    print("%s: %s" %(name, red("FAILED", out.fileno())))
    fail("In test %s:\\n%s" %(name, content))

# generate .cc -> binary with error check
def make_binary(cc: str, dir: str = ""):
    if dir:
        cc = join(dir, cc)

    name = splitext(cc)[0]

    # just compile if file doesn't exist
    try:
        t = getmtime(name)

        # if file not changed, don't compile back
        if t >= invariant_time and t >= getmtime(cc):
            return name

    except FileNotFoundError:
        pass

    cmd = MAKE_CMD + ("-o", name, cc, *o_compiled)

    p = Popen(cmd, stderr = PIPE)
    stderr = p.communicate()[1]

    if p.returncode != 0:
        fail_test(name, stderr.decode())

    if dir:
        join(dir, name)

    return name

# execute binary with error check
def exec(binary: str):
    # FIXME: some outputs are not passed (like print or err)
    p = Popen(("./%s" %binary), stdout = PIPE, stderr = PIPE)
    stdout, stderr = p.communicate()

    if p.returncode != 0:
        fail_test(binary, "\\n%s" %stderr.decode())

    print("%s: %s" %(binary, green("PASSED", out.fileno())))
    info("\\n%s" %stdout.decode())
"""

# generate tests.py, the file in which all tests are put
with open("tests.py", 'w') as f:
    print("MAKE_CMD = %s" %str(MAKE_CMD), file = f)
    print("o_compiled = %s" %o_compiled, file = f)
    print("invariant_time = %s" %invariant_time, file = f)

    print("")

    print(header, file = f)

    for file in listdir(TEST_DIR):
        parts = splitext(file)
        if parts[1] == ".cc":
            print("def test_%s():" %parts[0], file = f)
            print("\tbin = make_binary(\"%s\", \"%s\")" %(file, TEST_DIR), file = f)
            print("\texec(bin)\n", file = f)


run(["pytest",
     "-s",                              # disable capturing
     "--tb=no",                         # disable traceback (we only care about stderr)
     "--disable-pytest-warnings",      # disable internal warnings
     "--html=test_report.html",         # enable html output
     "tests.py"])

# rewrite the html report to transform ansi into html
with open("test_report.html") as f:
    report = f.read()

with open("test_report.html", 'w') as f:
    c = Converter()

    page = BeautifulSoup(report, "html.parser")
    head = page.find("head")

    for e in page.find_all("div"):
        converted = BeautifulSoup(unescape(c.convert(str(e))), "html.parser")
        head.contents.extend(converted.find("head").contents)
        e.contents = converted.find("div").contents

    f.write(str(page))