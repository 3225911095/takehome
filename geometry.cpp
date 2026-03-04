#include "geometry.h"
#include <cmath>
#include <algorithm>

using namespace std;
bool IsRectInsidePolygon(const Rect& r, const vector<Point>& poly)
{
	// 1. 获取矩形的四个角点
	array<Point, 4> arr;
	arr = RectCorners(r);

	// 2. 检查所有角点是否都在多边形内部
	for (int i = 0; i < arr.size(); i++)
	{
		if (!IsPointInPolygon(arr[i], poly))
		{
			return false;
		}
	}

	// 3. 检查矩形的边是否与多边形边相交
	// 矩形的四条边
	Segment rectEdges[4] = {
		{arr[0], arr[1]},  // 下边
		{arr[1], arr[2]},  // 右边
		{arr[2], arr[3]},  // 上边
		{arr[3], arr[0]}   // 左边
	};

	// 遍历多边形的每条边
	int polySize = poly.size();
	for (int i = 0; i < polySize; i++)
	{
		Segment polyEdge = {poly[i], poly[(i + 1) % polySize]};

		// 检查矩形边与多边形边是否严格相交
		for (int j = 0; j < 4; j++)
		{
			if (DoSegmentsProperIntersect(rectEdges[j], polyEdge))
			{
				// 如果有边相交，说明矩形不完全在内部
				return false;
			}
		}
	}

	// 所有角点都在内部且边不相交，返回 true
	return true;
}

//判定是否在一条线上
double Cross(const Point& a, const Point& b, const Point& c)
{
	return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}
//判定是否在判定区间内
bool Between(double value, double left, double right)
{
	return value >= left - EPS && value <= right + EPS;
}
//比较两个浮点数是否近似相等
bool NearlyEqual(double a, double b)
{
	return abs(a-b) < EPS;
}
//计算两点之间的欧氏距离
double Distance(const Point& a, const Point& b)
{
	double x = a.x - b.x;
	double y = a.y - b.y;
	return sqrt(x*x + y*y);
}
//规范化矩形，确保 x1 <= x2 且 y1 <= y2
Rect NormalizeRect(const Rect& r)
{
	Rect result = r;
	if (r.x1 > r.x2)
	{
		double temp = 0;
		temp = result.x1;
		result.x1 = result.x2;
		result.x2 = temp;
	}
	if (r.y1 > r.y2)
	{
		double temp = 0;
		temp = result.y1;
		result.y1 = result.y2;
		result.y2 = temp;
	}
	return result;
}
//思路：根据中心点和宽高创建矩形
Rect MakeRectFromCenter(const Point& c, double width_x, double height_y)
{
	Rect result = {};
	result.x1 = c.x - width_x / 2.0;
	result.x2 = c.x + width_x / 2.0;
	result.y1 = c.y - height_y / 2.0;
	result.y2 = c.y + height_y / 2.0;
	return result;
}
//计算点集的轴对齐包围盒
Rect ComputeBoundingBox(const vector<Point>& pts)
{
	Rect result = {};
	if (pts.empty())
	{
		return result;
	}
	result.x1 = pts[0].x;
	result.x2 = pts[0].x;
	result.y1 = pts[0].y;
	result.y2 = pts[0].y;
	for (int i = 0; i < pts.size(); i++)
	{
		if (result.x1 > pts[i].x)
		{
			result.x1 = pts[i].x;
		}
		if (result.x2 < pts[i].x)
		{
			result.x2 = pts[i].x;
		}
		if (result.y1 > pts[i].y)
		{
			result.y1 = pts[i].y;
		}
		if (result.y2 < pts[i].y)
		{
			result.y2 = pts[i].y;
		}
	}

	return result;
}
//扩展矩形的边界
Rect ExpandRect(const Rect& r, double dx, double dy)
{
	Rect result = r;
	result.x1 -= dx;
	result.x2 += dx;
	result.y1 -= dy;
	result.y2 += dy;
	return NormalizeRect(result);
}
//判断矩形是否为零矩形
bool IsZeroRect(const Rect& r)
{
	Rect result = r;
	if (RectWidth(result) <= 0)
	{
		return true;
	}
	if (RectHeight(result) <= 0)
	{
		return true;
	}
	return false;
}
//计算矩形的宽度
double RectWidth(const Rect& r)
{
	Rect result = r;
	double res = result.x2 - result.x1;
	return res;
}
//计算矩形的高度
double RectHeight(const Rect& r)
{
	Rect result = r;
	double res = result.y2 - result.y1;
	return res;
}
//获取矩形的四个角点,{Point(x1,y1), Point(x2,y1), Point(x2,y2), Point(x1,y2)}
array<Point, 4> RectCorners(const Rect& r)
{
	Rect result = r;
	array<Point, 4> res;
	result = NormalizeRect(result);
	res[0].x = result.x1;
	res[0].y = result.y1;
	res[1].x = result.x2;
	res[1].y = result.y1;
	res[2].x = result.x2;
	res[2].y = result.y2;
	res[3].x = result.x1;
	res[3].y = result.y2;
	return res;
}
//思路：判断两个矩形是否重叠
bool IsRectOverlap(const Rect& a, const Rect& b, bool allow_touch)
{
	// 分离条件: a在b的左边/右边/上边/下边
	bool separated_x = (a.x2 < b.x1) || (b.x2 < a.x1);
	bool separated_y = (a.y2 < b.y1) || (b.y2 < a.y1);
	
	if (allow_touch)
	{
		// 允许边接触: 分离条件使用严格<
		if (separated_x || separated_y)
		{
			return false;  // 分离,不重叠
		}
		return true;  // 重叠或接触
	}
	else
	{
		// 不允许边接触: 分离条件使用<=
		separated_x = (a.x2 <= b.x1) || (b.x2 <= a.x1);
		separated_y = (a.y2 <= b.y1) || (b.y2 <= a.y1);
		if (separated_x || separated_y)
		{
			return false;  // 分离,不重叠
		}
		return true;  // 重叠
	}
}
//判断线段是否是轴对齐的（水平或垂直）
bool IsAxisAligned(const Segment& s)
{
	if (NearlyEqual(s.a.x, s.b.x))
	{
		return true;
	}
	if (NearlyEqual(s.a.y, s.b.y))
	{
		return true;
	} 
	return false;
}
//判断点是否在线段上
bool IsPointOnSegment(const Point& p, const Segment& s)
{
	double crossVal = Cross(s.a, s.b, p);
	if (fabs(crossVal) > EPS)
	{
		return false;
	}
	double minX = std::min(s.a.x, s.b.x);
	double maxX = std::max(s.a.x, s.b.x);
	double minY = std::min(s.a.y, s.b.y);
	double maxY = std::max(s.a.y, s.b.y);
	if (!Between(p.x, minX, maxX)) 
	{
		return false;
	}
	if (!Between(p.y, minY, maxY))
	{
		return false;
	}
	return true;
}
//判断点是否在多边形内部
bool IsPointInPolygon(const Point& p, const vector<Point>& poly)
{
	if (poly.size() < 3)
	{
		return false;
	}
	int n = static_cast<int>(poly.size());
	for (int i = 0; i < n; i++)
	{
		Segment edge;
		edge.a = poly[i];
		edge.b = poly[(i + 1) % n];

		if (IsPointOnSegment(p, edge))
		{
			return true;
		}
	}
	int count = 0;
	for (int i = 0; i < n; i++)
	{
		Point p1 = poly[i];
		Point p2 = poly[(i + 1) % n];

		bool p1Above = (p1.y > p.y + EPS);
		bool p2Above = (p2.y > p.y + EPS);

		if (p1Above != p2Above)
		{
			double xIntersect = (p2.x - p1.x) * (p.y - p1.y) / (p2.y - p1.y) + p1.x;
			if (xIntersect > p.x + EPS)
			{
				count++;
			}
		}
	}
	return (count % 2) == 1;
}
//判断两条线段是否严格相交（不包括端点相接）
bool DoSegmentsProperIntersect(const Segment& s1, const Segment& s2)
{
	double d1 = Cross(s1.a, s1.b, s2.a);
	double d2 = Cross(s1.a, s1.b, s2.b);
	double d3 = Cross(s2.a, s2.b, s1.a);
	double d4 = Cross(s2.a, s2.b, s1.b);
	if (d1 * d2 < -EPS && d3 * d4 < -EPS)
	{
		return true;
	}
	return false;
}
