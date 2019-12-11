# include "python.hh"

int main()
{
    // Sequence getter
    auto list = Python::list(0, 1, 2, 3, 4, 5, 6);
    list[4].print();

    // Sequence setter
    list[5] = 42;
    list.print();

    // Dict getter
    auto sys = Python::import("sys");
    sys["stderr"].print();
}