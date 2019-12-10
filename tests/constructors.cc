# include "python.hh"

int main()
{
    auto sys = Python::import("sys");

    Python(std::string("This should work, nah ?")).print();
    Python("Don't you dare segfault on me").print();
    Python(L"Don't you dare segfault on me").print();
    Python(u"Don't you dare segfault on me").print();
    Python(U"Don't you dare segfault on me").print();
    Python('f').print();
    Python(std::filesystem::path("/perfectly/valid/path_or_not")).print();
    Python(0.001f).print();
    Python(0.001).print();
    Python(0.001l).print();
    Python(42ull).print();
    Python(42).print();
    Python(42l).print();
    Python(42ll).print();

    // True, False and None are not constructed, but shared
    Python(true).print();
    Python(false).print();

    Python({1, 2, 3, 4, 5, 6}).print();

    Python(sys["stderr"]).print();

    std::vector<double> a = {5, 9, 7, 2, 4};
    (void) Python(a);

    std::array<int, 10> b = {1, 9, 6, 7, 4, 2};
    Python(b).print();

    std::map<std::string, unsigned> c = {{"key1", 1},
                                         {"key2", 2},
                                         {"key3", 3},
                                         {"key4", 4}};
    Python(c).print();

    auto d = std::tuple("key1", 1, 0.5f, 42l);
    Python(d).print();

    // recursive construction
    std::vector<std::vector<std::pair<std::string, std::size_t>>> e =
    {
        { { "aa", 11 }, { "ba", 21 }, { "ca", 31 }, { "da", 41 }, { "ea", 51 } },
        { { "ab", 12 }, { "bb", 22 }, { "cb", 32 }, { "db", 42 }, { "eb", 52 } },
        { { "ac", 13 }, { "bc", 23 }, { "cc", 33 }, { "de", 43 }, { "ec", 53 } },
        { { "ad", 14 }, { "bd", 24 }, { "cd", 34 }, { "dd", 44 }, { "ed", 54 } },
        { { "ae", 15 }, { "be", 25 }, { "ce", 35 }, { "de", 45 }, { "ee", 55 } },
    };
    Python(e).print();

    std::vector<std::optional<int>> f = { std::optional(1), {}, std::optional(3), std::optional(4), {}};

    Python(f).print();
}