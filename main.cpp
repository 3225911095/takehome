#include <iostream>
#include <string>

#include "parser.h"
#include "solver.h"

using namespace std;

int main()
{
    Problem problem;
    string err;
    string filePath;
    cin >> filePath;
    if (!ParseProblemFromFile(filePath, problem, err))
    {
        cerr << "解析失败: " << err << endl;
        return 1;
    }
    Solver solver(problem);
    Solution solution = solver.Solve();
    cout << "============================" << endl;
    cout << "求解结果:" << endl;
    PrintSolution(problem, solution, cout);

    return 0;
}