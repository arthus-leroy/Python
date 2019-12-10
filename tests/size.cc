# include <cassert>

# include "python.hh"

int main()
{
    auto tuple = Python::tuple(5, 3, 4, 1, 2);
    auto list = Python::list(2, 3, 1);
    auto dict = Python::dict("a", 5, "b", 1, "c", 3, "d", 2, "e", 4, "f", 7, "g", 6);

    assert(tuple.size() == 5);
    assert(dict.size() == 7);
    assert(list.size() == 3);
}