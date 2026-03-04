#pragma once
#include "model.h"

#include <iosfwd>
#include <string>
#include <vector>

bool ParseProblemFromFile(const std::string& path, Problem& out, std::string& err);

void PrintSolution(const Problem& problem, const Solution& solution, std::ostream& os);

// 解析工具函数声明
std::vector<std::string> ReadAndPreprocessFile(const std::string& path, std::string& err);
int FindKeyword(const std::vector<std::string>& lines, const std::string& keyword);
std::vector<Point> ParsePoints(const std::vector<std::string>& lines, int startIndex, int endIndex);
DoorType ParseDoorType(const std::string& s);
ItemType ParseItemType(const std::string& typeStr);
bool ParseItem(const std::string& line, Item& out);