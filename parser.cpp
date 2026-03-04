#include "parser.h"
#include "utils.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <cctype>
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

static bool EndsWithIgnoreCase(const std::string& s, const std::string& suffix)
{
    if (s.size() < suffix.size()) return false;
    std::string a = s.substr(s.size() - suffix.size());
    return ToUpperCopy(a) == ToUpperCopy(suffix);
}

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
    if (EndsWithIgnoreCase(path, ".json"))
    {
        return ParseProblemFromJsonFile(path, out, err);
    }

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

bool ParseProblemFromJsonFile(const std::string& path, Problem& out, std::string& err)
{
    std::ifstream file(path.c_str());
    if (!file.is_open())
    {
        err = "无法打开文件: " + path;
        return false;
    }

    json j;
    try
    {
        file >> j;
    }
    catch (const std::exception& e)
    {
        err = std::string("JSON 解析失败: ") + e.what();
        return false;
    }

    Problem temp;

    // 1. boundary
    if (!j.contains("boundary") || !j["boundary"].is_array())
    {
        err = "缺少 boundary 数组";
        return false;
    }

    for (const auto& p : j["boundary"])
    {
        if (!p.is_array() || p.size() != 2)
        {
            err = "boundary 点格式错误，应为 [x,y]";
            return false;
        }

        Point pt;
        pt.x = p[0].get<double>();
        pt.y = p[1].get<double>();
        temp.contour.push_back(pt);
    }

    if (temp.contour.size() < 3)
    {
        err = "boundary 点数不足";
        return false;
    }

    // 若首尾重复闭合，删掉最后一个重复点（你的样例里就是这样）
    if (temp.contour.size() >= 2)
    {
        const Point& first = temp.contour.front();
        const Point& last = temp.contour.back();
        if (NearlyEqual(first.x, last.x) && NearlyEqual(first.y, last.y))
        {
            temp.contour.pop_back();
        }
    }

    // 2. door
    if (!j.contains("door") || !j["door"].is_array() || j["door"].size() != 2)
    {
        err = "door 格式错误，应为 [[x1,y1],[x2,y2]]";
        return false;
    }

    for (int i = 0; i < 2; ++i)
    {
        if (!j["door"][i].is_array() || j["door"][i].size() != 2)
        {
            err = "door 点格式错误";
            return false;
        }
    }

    temp.door.seg.a.x = j["door"][0][0].get<double>();
    temp.door.seg.a.y = j["door"][0][1].get<double>();
    temp.door.seg.b.x = j["door"][1][0].get<double>();
    temp.door.seg.b.y = j["door"][1][1].get<double>();

    // 3. isOpenInward
    if (!j.contains("isOpenInward") || !j["isOpenInward"].is_boolean())
    {
        err = "缺少 isOpenInward 布尔字段";
        return false;
    }

    temp.door.type = j["isOpenInward"].get<bool>() ? DoorType::INWARD : DoorType::OUTWARD;

    // 4. algoToPlace
    if (!j.contains("algoToPlace") || !j["algoToPlace"].is_object())
    {
        err = "缺少 algoToPlace 对象";
        return false;
    }

    for (auto it = j["algoToPlace"].begin(); it != j["algoToPlace"].end(); ++it)
    {
        const std::string name = it.key();
        const json& sizeArr = it.value();

        if (!sizeArr.is_array() || sizeArr.size() != 2)
        {
            err = "algoToPlace 中物品尺寸格式错误，应为 [length,width]";
            return false;
        }

        Item item;
        item.name = name;
        item.type = ParseItemTypeFromName(name);
        item.length = sizeArr[0].get<double>();
        item.width = sizeArr[1].get<double>();

        temp.items.push_back(item);
    }

    if (temp.items.empty())
    {
        err = "algoToPlace 为空";
        return false;
    }

    out = temp;
    return true;
}

ItemType ParseItemTypeFromName(const std::string& itemName)
{
    std::string s = ToUpperCopy(itemName);

    if (s == "FRIDGE")
    {
        return ItemType::FRIDGE;
    }
    if (s == "ICEMAKER" || s == "ICE_MAKER")
    {
        return ItemType::ICEMAKER;
    }
    if (s.find("OVERSHELF") == 0 || s.find("OVER_SHELF") == 0)
    {
        return ItemType::OVERSHELF;
    }
    if (s.find("SHELF") == 0)
    {
        return ItemType::SHELF;
    }

    // 默认兜底
    return ItemType::SHELF;
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
