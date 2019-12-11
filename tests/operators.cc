

# include "python.hh"

int main()
{
    // normal use
    std::cout << "16 + 26 =     ";  (Python(16) + Python(26)).print();
    std::cout << "57 - 12 =     ";  (Python(57) - Python(15)).print();
    std::cout << "6 * 7 =       ";  (Python(6) * Python(7)).print();
    std::cout << "504 / 12 =    ";  (Python(504) / Python(12)).print();
    std::cout << "702 % 66 =    ";  (Python(702) % Python(66)).print();
    std::cout << "672 >> 4 =    ";  (Python(672) >> Python(4)).print();
    std::cout << "21 << 1 =     ";  (Python(21) << Python(1)).print();
    std::cout << "234 & 63 =    ";  (Python(234) & Python(63)).print();
    std::cout << "255 ^ 213 =   ";  (Python(255) ^ Python(213)).print();
    std::cout << "32 | 8 | 2 =  ";  (Python(32) | Python(8) | Python(2)).print();

    std::cout << "\n";

    // inplace
    auto a = Python(16);
    auto b = Python(57);
    auto c = Python(6);
    auto d = Python(504);
    auto e = Python(702);
    auto f = Python(672);
    auto g = Python(21);
    auto h = Python(234);
    auto i = Python(255);
    auto j = Python(32);
   
    std::cout << "16 + 26 =     ";  a += Python(26);    a.print();
    std::cout << "57 - 12 =     ";  b -= Python(15);    b.print();
    std::cout << "6 * 7 =       ";  c *= Python(7);     c.print();
    std::cout << "504 / 12 =    ";  d /= Python(12);    d.print();
    std::cout << "702 % 66 =    ";  e %= Python(66);    e.print();
    std::cout << "672 >> 4 =    ";  f >>= Python(4);    f.print();
    std::cout << "21 << 1 =     ";  g <<= Python(1);    g.print();
    std::cout << "234 & 63 =    ";  h &= Python(63);    h.print();
    std::cout << "255 ^ 213 =   ";  i ^= Python(213);   i.print();
    std::cout << "32 | 8 | 2 =  ";  j |= Python(8) |= Python(2);    j.print();

    // concat
    std::cout << "Hello World: ";   (Python("Hello") + Python(' ') + Python("World")).print();
    std::cout << "(1-4) + (5-8): "; (Python::tuple(1, 2, 3, 4) + Python::tuple(5, 6, 7, 8)).print();
    std::cout << "[1-4] + [5-8]: "; (Python::list(1, 2, 3, 4) + Python::list(5, 6, 7, 8)).print();
}