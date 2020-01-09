# include "python.hh"

int main()
{
    std::cout << "This should work, nah ? => ";
    Python(std::string("This should work, nah ?")).print();
    
    std::cout << "Don't you dare segfault on me => ";
    Python("Don't you dare segfault on me").print();

    std::cout << "Don't you dare segfault on me => ";
    Python(L"Don't you dare segfault on me").print();

    std::cout << "Don't you dare segfault on me => ";
    Python(u"Don't you dare segfault on me").print();

    std::cout << "Don't you dare segfault on me => ";
    Python(U"Don't you dare segfault on me").print();

    std::cout << "f => ";
    Python('f').print();

    std::cout << "/perfectly/valid/path_or_not => ";
    Python(std::filesystem::path("/perfectly/valid/path_or_not")).print();

    std::cout << "0.001 => ";
    Python(0.001f).print();

    std::cout << "0.001 => ";
    Python(0.001).print();

    std::cout << "0.001 => ";
    Python(0.001l).print();

    std::cout << "42 => ";
    Python(42ull).print();

    std::cout << "42 => ";
    Python(42).print();

    std::cout << "42 => ";
    Python(42l).print();

    std::cout << "42 => ";
    Python(42ll).print();

    // True, False and None are not constructed, but shared
    std::cout << "True => ";
    Python(true).print();

    std::cout << "False => ";
    Python(false).print();

    std::cout << "[1, 2, 3, 4, 5, 6] => ";
    Python({1, 2, 3, 4, 5, 6}).print();

    std::cout << "sys[stderr] => ";
    Python(Python::import("sys")["stderr"]).print();


    std::vector<double> a = {5, 9, 7, 2, 4};
    std::cout << "[5, 9, 7, 2, 4] => ";
    Python(a).print();

    std::array<int, 10> b = {1, 9, 6, 7, 4, 2};
    std::cout << "[1, 9, 7, 2, 4, 0, 0, 0, 0] => ";
    Python(b).print();

    std::map<std::string, unsigned> c = {{"key1", 1},
                                         {"key2", 2},
                                         {"key3", 3},
                                         {"key4", 4}};
    std::cout << "{key1: 1, key2: 2, key3: 3, key4: 4} => ";
    Python(c).print();

    // c++ tuple -> tuple
    auto d1 = Python::tuple(std::tuple("key1", 1, 0.5f, 42l));
    std::cout << "(\"key1\", 1, 0.5, 42) => ";
    d1.print();

    // c++ tuple -> list
    auto d2 = Python::list(std::tuple("key1", 1, 0.5f, 42l));
    std::cout << "(\"key1\", 1, 0.5, 42) => ";
    d2.print();

    // tuple -> tuple
    std::cout << "(\"key1\", 1, 0.5, 42) => ";
    Python::tuple(d1).print();

    // tuple -> list
    std::cout << "[\"key1\", 1, 0.5, 42] => ";
    Python::list(d2).print();

    // list -> tuple
    std::cout << "(\"key1\", 1, 0.5, 42) => ";
    Python::tuple(d1).print();

    // list -> list
    std::cout << "[\"key1\", 1, 0.5, 42] => ";
    Python::list(d2).print();

    // tuple -> set
    std::cout << "{\"key1\", 1, 0.5, 42} => ";
    Python::set(d1).print();

    // list -> set
    std::cout << "{\"key1\", 1, 0.5, 42} => ";
    Python::set(d2).print();

    // keys, values -> dict
    std::cout << "{key1: \"key1\", 1: 1, 0.5: 0.5, 42: 42} => ";
    Python::dict(d1, d2).print();

    // recursive construction
    std::vector<std::vector<std::pair<std::string, std::size_t>>> nested_containers =
    {
        { { "aa", 11 }, { "ba", 21 }, { "ca", 31 }, { "da", 41 }, { "ea", 51 } },
        { { "ab", 12 }, { "bb", 22 }, { "cb", 32 }, { "db", 42 }, { "eb", 52 } },
        { { "ac", 13 }, { "bc", 23 }, { "cc", 33 }, { "de", 43 }, { "ec", 53 } },
        { { "ad", 14 }, { "bd", 24 }, { "cd", 34 }, { "dd", 44 }, { "ed", 54 } },
        { { "ae", 15 }, { "be", 25 }, { "ce", 35 }, { "de", 45 }, { "ee", 55 } },
    };
    auto star1 = Python(nested_containers);
    std::cout << "[(\"aa\", 11), (\"ba\", 21), (\"ca\", 31), (\"da\", 41), (\"ea\", 51)],"
                 "[(\"ab\", 12), (\"bb\", 22), (\"cb\", 32), (\"db\", 42), (\"eb\", 52)],"
                 "[(\"ac\", 13), (\"bc\", 23), (\"cc\", 33), (\"dc\", 43), (\"ec\", 53)],"
                 "[(\"ad\", 14), (\"bd\", 24), (\"cd\", 34), (\"dd\", 44), (\"ed\", 54)],"
                 "[(\"ae\", 15), (\"be\", 25), (\"ce\", 35), (\"de\", 45), (\"ee\", 55)] => ";
    star1.print();

    std::vector<std::optional<int>> f = { std::optional(1), {}, std::optional(3), std::optional(4), {}};
    std::cout << "[1, None, 3, 4, None] => ";
    Python(f).print();

    std::cout << "(1, 2, 1, 2, 3, 4, \"thing\", *star1, 5.5) => ";
    Python::tuple(1, 2, Python::Starred(Python::tuple(1, 2, 3, 4)), "thing", Python::Starred(star1), 5.5).print();

    std::cout << "[1, 2, 1, 2, 3, 4, \"thing\", *star1, 5.5] => ";
    Python::list(1, 2, Python::Starred(Python::list(1, 2, 3, 4)), "thing", Python::Starred(star1), 5.5).print();
}