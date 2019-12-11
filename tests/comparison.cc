# include "python.hh"

int main()
{
    auto a = Python::tuple(1, 2, 3, 4, 5, 6);
    auto b = Python::tuple(1, 2, 3, 4, 5, 6);
    assert(a == b);

    auto c = Python::tuple(1, 2, 3, 4, 5);
    assert(c < a);

    auto d = Python::list(1, 2, 3, 4, 5);
    assert(c != d);
}