# include "python.hh"

int main()
{
    auto sys = Python::import("sys");

    Python(std::string("This should work, nah ?"));
    Python("Don't you dare segfault on me");
    Python('f');
    Python(std::filesystem::path("/perfectly/valid/path_or_not"));
    Python(0.001f);
    Python(0.001);
    Python(0.001l);
    Python(42ull);
    Python(42);
    Python(42l);
    Python(42ll);

    // True, False and None are not constructed, but shared
    Python(true);

    Python({1, 2, 3, 4, 5, 6});

    // the (void) are required, otherwise it's considered as a variable declaration
    (void) Python(sys["stderr"]);

    std::vector<double> a = {5, 9, 7, 2, 4};
    (void) Python(a);

    std::array<int, 10> b = {1, 9, 6, 7, 4, 2};
    (void) Python(b);

    std::map<std::string, unsigned> c = {{"key1", 1},
                                         {"key2", 2},
                                         {"key3", 3},
                                         {"key4", 4}};

    (void) Python(c);
}