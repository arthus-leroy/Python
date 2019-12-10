// TEST: import, tuple (complex form), dict (complex form), builtins, call

# include "python.hh"

int main()
{
    auto sys = Python::import("sys");

    auto args = Python::tuple("this", "is", "a", "normal", "output");
    auto kwargs = Python::dict("file", sys["stderr"],
                               "sep" , ",",
                               "end" , "\nhere\nis\nthe\nend\n");

    Python::call_builtin("print", args, kwargs);
}