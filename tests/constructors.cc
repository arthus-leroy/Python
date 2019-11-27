# include "python.hh"

int main()
{
    auto sys = Python::import("sys");

    Python(std::string("This should work, nah ?"));
    Python("Don't you dare segfault on me");
    Python(std::filesystem::path("/perfectly/valid/path_or_not"));
    Python(0.001f);
    Python(0.001);
    Python(0.001l);
    Python(42ull);
    Python(42);
    Python(42l);
    Python(42ll);
    // FIXME: Python(sys["stderr"]) fails to compile miserably
    Python{sys["stderr"]};
    Python(true);
}