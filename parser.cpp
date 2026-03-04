#include "parser.h"
#include "utils.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <cctype>

using namespace std;

vector<string> ReadAndPreprocessFile(const string& path, string& err)
{
    ifstream file(path.c_str());
    if (!file.is_open())
    {
        err = "无法打开文件: " + path;
        return vector<string>();
    }
    vector<string> lines;
    string line;
    while (getline(file, line))
    {
        size_t pos = line.find("//");
        if (pos != string::npos)
        {
            line = line.substr(0, pos);
        }
        line = Trim(line);
        if (!line.empty())
        {
            lines.push_back(line);
        }
    }

    file.close();
    return lines;
}

// 查找关键字所在行号
int FindKeyword(const vector<string>& lines, const string& keyword)
{
    string upperKeyword = ToUpperCopy(keyword);

    for (int i = 0; i < lines.size(); i++)
    {
        string upperLine = ToUpperCopy(lines[i]);
        if (upperLine.find(upperKeyword) != string::npos)
        {
            return i;
        }
    }

    return -1;
}

// 从指定行开始解析点坐标，返回解析的点的向量
vector<Point> ParsePoints(const vector<string>& lines, int startIndex, int endIndex)
{
    vector<Point> points;

    for (int i = startIndex; i < endIndex && i < lines.size(); i++)
    {
        string line = Trim(lines[i]);
        if(line.empty())
        {
            continue;
        }
        vector<string> parts = SplitWhitespace(line);
        if (parts.size() >= 2 && (isdigit(parts[0][0]) || parts[0][0] == '-'))
        {
            Point pt;
            pt.x = atof(parts[0].c_str());
            pt.y = atof(parts[1].c_str());
            points.push_back(pt);
        }
        else
        {
            break;
        }
    }

    return points;
}

DoorType ParseDoorType(const std::string& s)
{
    string text = ToUpperCopy(s);
    if (text == "INWARD") 
    {
        return DoorType::INWARD;
    }
    if (text == "OUTWARD")
    {
        return DoorType::OUTWARD;
    }
    return DoorType::INWARD;
}

// 解析家具类型字符串
ItemType ParseItemType(const string& typeStr)
{
    string upperType = ToUpperCopy(typeStr);

    if (upperType == "FRIDGE")
    {
        return ItemType::FRIDGE;
    }
    else if (upperType == "SHELF")
    {
        return ItemType::SHELF;
    }
    else if (upperType == "OVERSHELF")
    {
        return ItemType::OVERSHELF;
    }
    else if (upperType == "ICEMAKER")
    {
        return ItemType::ICEMAKER;
    }
    return ItemType::SHELF;
}

// 解析单个家具行
bool ParseItem(const string& line, Item& out)
{
    vector<string> parts = SplitWhitespace(line);
    if (parts.size() < 4)
    {
        return false;
    }
    out.name = parts[0];
    out.type = ParseItemType(parts[1]);
    out.length = atof(parts[2].c_str());
    out.width = atof(parts[3].c_str());

    return true;
}

bool ParseProblemFromFile(const string& path, Problem& out, string& err)
{
    vector<string> lines = ReadAndPreprocessFile(path, err);
    if (lines.empty() && !err.empty())
    {
        return false;
    }
    Problem temp;
    int index = 0;
    vector<string> parts;
    if (index >= lines.size())
    {
        err = "缺少CONTOUR";
        return false;
    }
    parts = SplitWhitespace(lines[index]);
    index++;

    if (parts.size() != 2)
    {
        err = "CONTOUR段格式错误";
        return false;
    }
    if (ToUpperCopy(parts[0]) != "CONTOUR")
    {
        err = "第一段应为CONTOUR";
        return false;
    }

    int pointCount = 0;
    {
        istringstream iss(parts[1]);
        iss >> pointCount;
        if (iss.fail() || !iss.eof() || pointCount < 3)
        {
            err = "CONTOUR点数不合法";
            return false;
        }
    }

    for (int i = 0; i < pointCount; i++)
    {
        if (index >= static_cast<int>(lines.size()))
        {
            err = "轮廓点数量不足";
            return false;
        }

        parts = SplitWhitespace(lines[index]);
        index++;

        if (parts.size() != 2)
        {
            err = "轮廓点格式错误";
            return false;
        }

        Point pt;
        pt.x = atof(parts[0].c_str());
        pt.y = atof(parts[1].c_str());

        temp.contour.push_back(pt);
    }

    if (temp.contour.size() < 3)
    {
        err = "ROOM特征不够";
        return false;
    }

    if (index >= static_cast<int>(lines.size()))
    {
        err = "缺少 DOOR 段";
        return false;
    }

    parts = SplitWhitespace(lines[index]);
    index++;

    if (parts.size() != 1 || ToUpperCopy(parts[0]) != "DOOR")
    {
        err = "应存在单独一行 DOOR";
        return false;
    }

    if (index >= static_cast<int>(lines.size()))
    {
        err = "DOOR数据不完整";
        return false;
    }

    parts = SplitWhitespace(lines[index]);
    index++;

    if (parts.size() != 5)
    {
        err = "DOOR 数据格式错误，应为: x1 y1 x2 y2 INWARD/OUTWARD";
        return false;
    }

    temp.door.seg.a.x = atof(parts[0].c_str());
    temp.door.seg.a.y = atof(parts[1].c_str());
    temp.door.seg.b.x = atof(parts[2].c_str());
    temp.door.seg.b.y = atof(parts[3].c_str());
    temp.door.type = ParseDoorType(parts[4]);

    if (index >= static_cast<int>(lines.size()))
    {
        err = "缺少 ITEMS 段";
        return false;
    }

    parts = SplitWhitespace(lines[index]);
    index++;

    if (parts.size() != 2)
    {
        err = "ITEMS 段格式错误，应为: ITEMS 数量";
        return false;
    }

    if (ToUpperCopy(parts[0]) != "ITEMS")
    {
        err = "应存在 ITEMS 段";
        return false;
    }

    int itemCount = 0;
    {
        istringstream iss(parts[1]);
        iss >> itemCount;
        if (iss.fail() || !iss.eof() || itemCount < 0)
        {
            err = "ITEMS 数量不合法";
            return false;
        }
    }

    for (int i = 0; i < itemCount; i++)
    {
        if (index >= static_cast<int>(lines.size()))
        {
            err = "物体数量不足";
            return false;
        }

        Item item;
        if (!ParseItem(lines[index], item))
        {
            err = "物体行格式错误，应为: name type length width";
            return false;
        }

        temp.items.push_back(item);
        index++;
    }
    out = temp;
    return true;
}

void PrintSolution(const Problem& problem, const Solution& solution, ostream& os)
{
    os << fixed << setprecision(3);
    if (!solution.feasible)
    {
        os << "FEASIBLE NO" << endl;
        return;
    }
    os << "FEASIBLE YES" << endl;
    for (int i = 0; i < static_cast<int>(solution.placements.size()); i++)
    {
        const Placement& p = solution.placements[i];
        if (p.item_index < 0 || p.item_index >= static_cast<int>(problem.items.size()))
        {
            continue;
        }
        os << problem.items[p.item_index].name
            << " center=("
            << p.center.x << ", "
            << p.center.y << ") "
            << "angle=" << p.angle
            << endl;
    }
}
