# include "python.hh"

int main()
{
    // Sequence
    auto list = Python::list(0, 1, 2, 3, 4, 5, 6);
    Python four = list[4];

    // Dict
    auto sys = Python::import("sys");
    Python stderr = sys["stderr"];
}