# include <cassert>
# include <iostream>

# include "python.hh"

int main()
{
    std::cout << "TUPLE" << std::endl;
    auto tuple = Python::tuple(5, 3, 4, 1, 2);
    std::cout << "LIST" << std::endl;
    auto list = Python::list(2, 3, 1);
    std::cout << "DICT" << std::endl;
    auto dict = Python::dict("a", 5, "b", 1, "c", 3, "d", 2, "e", 4, "f", 7, "g", 6);

    std::cout << "TEST 1" << std::endl;
    assert(tuple.size() == 5);
    std::cout << "TEST 3" << std::endl;
    assert(dict.size() == 7);
    std::cout << "TEST 2" << std::endl;
    assert(list.size() == 3);
}