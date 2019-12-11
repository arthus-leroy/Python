

# include "python.hh"

int main()
{
    std::cout << "16 + 26 =     ";  (Python(16) + Python(26)).print();
    std::cout << "57 - 12 =     ";  (Python(57) - Python(15)).print();
    std::cout << "6 * 7 =       ";  (Python(6) * Python(7)).print();
    std::cout << "504 / 12 =    ";  (Python(504) / Python(12)).print();
    std::cout << "702 % 66 =    ";  (Python(702) % Python(66)).print();
    std::cout << "672 >> 4 =    ";  (Python(672) >> Python(4)).print();
    std::cout << "21 << 1 =     ";  (Python(21) << Python(1)).print();
    std::cout << "234 & 63 =    ";  (Python(234) & Python(63)).print();
    // weirdly, this one segfault on a dealloction error : bug ?
    std::cout << "255 ^ 213 =   ";  (Python(255) ^ Python(213)).print();
    // any number between 132 and 255
//    std::cout << "251 ^ 209 =   ";  (Python(251) ^ Python(209)).print();
    std::cout << "32 | 8 | 2 =  ";  (Python(32) | Python(8) | Python(2)).print();
}