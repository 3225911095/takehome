#pragma once
#include <array>
#include <vector>

//误差范围
constexpr double EPS = 1e-9;

struct Point {
    double x = 0.0;
    double y = 0.0;
};

struct Segment {
    Point a;
    Point b;
};

struct Rect {
    double x1 = 0.0;
    double y1 = 0.0;
    double x2 = 0.0;
    double y2 = 0.0;
};

double Cross(const Point& a, const Point& b, const Point& c);

bool Between(double value, double left, double right);

bool NearlyEqual(double a, double b);

double Distance(const Point& a, const Point& b);

Rect NormalizeRect(const Rect& r);

Rect MakeRectFromCenter(const Point& c, double width_x, double height_y);

Rect ComputeBoundingBox(const std::vector<Point>& pts);

Rect ExpandRect(const Rect& r, double dx, double dy);

bool IsZeroRect(const Rect& r);

double RectWidth(const Rect& r);

double RectHeight(const Rect& r);

std::array<Point, 4> RectCorners(const Rect& r);

bool IsRectOverlap(const Rect& a, const Rect& b, bool allow_touch = true);

bool IsAxisAligned(const Segment& s);

bool IsPointOnSegment(const Point& p, const Segment& s);

bool IsPointInPolygon(const Point& p, const std::vector<Point>& poly);

bool DoSegmentsProperIntersect(const Segment& s1, const Segment& s2);

bool IsRectInsidePolygon(const Rect& r, const std::vector<Point>& poly);