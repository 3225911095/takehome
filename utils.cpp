#include "utils.h"
#include <algorithm>
#include <cctype>
#include <sstream>

using namespace std;

// 先找出非空白行的部分
std::string Trim(const std::string& s)
{
    int left = 0;
    int right = s.size() - 1;
    while (left < right && isspace(s[left]))
    {
        left++;
    }
    while (right > left && isspace(s[right]))
    {
        right--;
    }
    if (left > right)
    {
        return "";
    }

    return s.substr(left, right - left + 1);
}

//按空白字符分割字符串，返回子字符串向量
vector<string> SplitWhitespace(const string& s)
{
    vector<string> result;
    string temp;
    istringstream iss(s);
    while (iss >> temp)
    {
        result.push_back(temp);
    }
    return result;
}

//返回字符串的大写形式副本
std::string ToUpperCopy(const std::string& s)
{
    string res = s;
    for (char& c : res)
    {
        c = toupper(c);
    }
    return res;
}
