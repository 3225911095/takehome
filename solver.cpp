#include "solver.h"
#include "geometry.h"
#include <algorithm>
#include <iostream>

using namespace std;

Solver::Solver(const Problem& problem)
    : problem_(problem)
{
    room_bbox_ = ComputeBoundingBox(problem_.contour);
    door_block_ = BuildDoorBlock();
}

Solution Solver::Solve() const
{
    Solution ans;
    ans.feasible = false;

    // 第一阶段：仅贴墙候选
    {
        vector<vector<Candidate>> all_candidates(problem_.items.size());

        for (int i = 0; i < static_cast<int>(problem_.items.size()); i++)
        {
            all_candidates[i] = GenerateWallCandidates(i);
            cout << "[调试] Item " << i << " (" << problem_.items[i].name << ") 贴墙候选数: " << all_candidates[i].size() << endl;
        }

        if (TrySolveWithCandidates(all_candidates, &ans))
        {
            return ans;
        }
        cout << "[调试] 第一阶段失败，尝试添加内部候选" << endl;
    }

    // 第二阶段：贴墙 + 内部候选
    {
        vector<vector<Candidate>> all_candidates(problem_.items.size());

        for (int i = 0; i < static_cast<int>(problem_.items.size()); i++)
        {
            all_candidates[i] = GenerateWallCandidates(i);

            vector<Candidate> extra = GenerateInteriorCandidates(i, 0.0);

            for (int j = 0; j < static_cast<int>(extra.size()); j++)
            {
                AddCandidateUnique(all_candidates[i], extra[j]);
            }
            cout << "[调试] Item " << i << " (" << problem_.items[i].name << ") 总候选数: " << all_candidates[i].size() << endl;
        }

        if (TrySolveWithCandidates(all_candidates, &ans))
        {
            return ans;
        }
        cout << "[调试] 第二阶段也失败" << endl;
    }

    ans.feasible = false;
    ans.placements.clear();
    return ans;
}

double Solver::RotatedSpanX(const Item& item, int angle) const
{
    if (angle == 0)
    {
        return item.length;
    }
    else
    {
        return item.width;
    }
}

double Solver::RotatedSpanY(const Item& item, int angle) const
{
    if (angle == 0)
    {
        return item.width;
    }
    else
    {
        return item.length;
    }
}

Rect Solver::BuildDoorBlock() const
{
    double doorWidth = Distance(problem_.door.seg.a, problem_.door.seg.b);
    double depth = 0.0;
    if (problem_.door.type == DoorType::INWARD)
    {
        depth = doorWidth;
    }
    else
    {
        depth = 1;
    }
    //房间包围盒中心点和门所在位置对比
    Point roomcenter = {};
    roomcenter.x = (room_bbox_.x1 + room_bbox_.x2) / 2.0;
    roomcenter.y = (room_bbox_.y1 + room_bbox_.y2) / 2.0;

    //通过水平或竖直判定拓展方向
    if (NearlyEqual(problem_.door.seg.a.y, problem_.door.seg.b.y))
    {
        double y = problem_.door.seg.a.y;
        double x1 = min(problem_.door.seg.a.x, problem_.door.seg.b.x);
        double x2 = max(problem_.door.seg.a.x, problem_.door.seg.b.x);

        Rect res;
        if (roomcenter.y >= y)
        {
            res.x1 = x1;
            res.x2 = x2;
            res.y1 = y;
            res.y2 = y + depth;
        }
        else
        {
            res.x1 = x1;
            res.x2 = x2;
            res.y1 = y - depth;
            res.y2 = y ;
        }
        return NormalizeRect(res);
    }
    if (NearlyEqual(problem_.door.seg.a.x, problem_.door.seg.b.x))
    {
        double x = problem_.door.seg.a.x;
        double y1 = min(problem_.door.seg.a.y, problem_.door.seg.b.y);
        double y2 = max(problem_.door.seg.a.y, problem_.door.seg.b.y);

        Rect res;
        if (roomcenter.x >= x)
        {
            res.x1 = x;
            res.x2 = x + depth;
            res.y1 = y1;
            res.y2 = y2 ;
        }
        else
        {
            res.x1 = x - depth;
            res.x2 = x;
            res.y1 = y1;
            res.y2 = y2;
        }
        return NormalizeRect(res);
    }
    return Rect{};
}

Rect Solver::BuildFridgeForbiddenZone(const Rect& body, int angle, const Item& item) const
{
    double forbidden = item.width;
    Rect res;
    if (angle == 0)
    {
        res.x1 = body.x1;
        res.x2 = body.x2;
        res.y1 = body.y1;
        res.y2 = body.y2 + forbidden;
    }
    else if (angle == 90)
    {
        res.x1 = body.x1;
        res.x2 = body.x2 + forbidden;
        res.y1 = body.y1;
        res.y2 = body.y2;
    }
    return NormalizeRect(res);
}

Placement Solver::BuildPlacement(int item_index, const Solver::Candidate& c) const
{
    Placement place = {};
    place.item_index = item_index;
    place.center = c.center;
    place.body = c.body;
    place.angle = c.angle;
    const Item& item = problem_.items[item_index];
    if (item.type == ItemType::FRIDGE)
    {
        place.has_forbidden_zone = true;
        place.forbidden_zone = BuildFridgeForbiddenZone(c.body, c.angle, item);
    }
    else
    {
        place.has_forbidden_zone = false;
    }
    return place;
}

bool Solver::BasicCandidateValid(const Solver::Candidate& c) const
{
    if (!IsRectInsidePolygon(c.body, problem_.contour))
    {
        return false;
    }
    // 家具不能与门区域重叠（包括边接触）
    if (!IsZeroRect(door_block_))
    {
        if (IsRectOverlap(c.body, door_block_, false))
        {
            return false;
        }
    }
    return true;
}

bool Solver::IsPlacementValid(const Placement& p, const std::vector<Placement>& placed) const
{
    if (!IsRectInsidePolygon(p.body,problem_.contour))
    {
        return false;
    }
    // 家具不能与门区域重叠（包括边接触）
    if (!IsZeroRect(door_block_) && IsRectOverlap(p.body, door_block_, false))
    {
        return false;
    }
    if (p.has_forbidden_zone == true)
    {
        if (!IsRectInsidePolygon(p.forbidden_zone, problem_.contour))
        {
            return false;
        }
    }
    for (int i = 0; i < placed.size(); i++)
    {
        const Placement& place = placed[i];
        // 家具之间不能重叠（包括边接触）
        if (IsRectOverlap(p.body, place.body, false))
        {
            return false;
        }
        // 不能进入其他家具的禁止区域
        if (place.has_forbidden_zone && IsRectOverlap(p.body, place.forbidden_zone, false))
        {
            return false;
        }
        if (p.has_forbidden_zone && IsRectOverlap(p.forbidden_zone, place.body, false))
        {
            return false;
        }
    }
    return true;
}

void Solver::AddCandidateUnique(std::vector<Solver::Candidate>& out, const Solver::Candidate& c) const
{
    for (int i = 0; i < out.size(); i++)
    {
        if (out[i].angle == c.angle && NearlyEqual(out[i].center.x, c.center.x) && NearlyEqual(out[i].center.y, c.center.y))
        {
            return;
        }       
    }
    out.push_back(c);
}

double Solver::ComputeAdaptiveStep(const Item& item, int angle, bool wall_mode) const
{
    double w = RotatedSpanX(item, angle);
    double h = RotatedSpanY(item, angle);

    double base = std::min(w, h);
    if (base <= EPS)
    {
        return default_grid_step_;
    }
    double step = 0.0;
    if (wall_mode)
    {
        // 贴墙采样
        step = base * 0.5;
    }
    else
    {
        // 内部采样
        step = base * 0.35;
    }
    // 上下限
    if (step < 50.0)
    {
        step = 50.0;
    }
    if (step > 250.0)
    {
        step = 250.0;
    }
    return step;
}

void Solver::AppendLineSamples(double low, double high, double half_span, double step, std::vector<double>& out) const
{
    if (step <= EPS)
    {
        step = default_grid_step_;
    }
    double start = low + half_span;
    double end = high - half_span;
    if (start > end + EPS)
    {
        return;
    }
    out.push_back(start);
    for (double v = start + step; v < end - EPS; v += step)
    {
        out.push_back(v);
    }
    if (!NearlyEqual(start, end))
    {
        out.push_back(end);
    }
}

bool Solver::InferInteriorNormal(const Segment& edge, double* nx, double* ny) const
{
    if (nx == nullptr || ny == nullptr)
    {
        return false;
    }

    *nx = 0.0;
    *ny = 0.0;

    Point mid;
    mid.x = (edge.a.x + edge.b.x) / 2.0;
    mid.y = (edge.a.y + edge.b.y) / 2.0;

    double probe = std::min(RectWidth(room_bbox_), RectHeight(room_bbox_)) * 1e-4;
    if (probe < 1e-3)
    {
        probe = 1e-3;
    }

    Point roomcenter = {};
    roomcenter.x = (room_bbox_.x1 + room_bbox_.x2) / 2.0;
    roomcenter.y = (room_bbox_.y1 + room_bbox_.y2) / 2.0;

    // 水平边
    if (NearlyEqual(edge.a.y, edge.b.y))
    {
        Point above;
        above.x = mid.x;
        above.y = mid.y + probe;
        Point below;
        below.x = mid.x; 
        below.y = mid.y - probe;
        bool inAbove = IsPointInPolygon(above, problem_.contour);
        bool inBelow = IsPointInPolygon(below, problem_.contour);

        if (inAbove != inBelow)
        {
            *nx = 0.0;
            *ny = inAbove ? 1.0 : -1.0;
            return true;
        }

        // 兜底：用房间中心判断
        *nx = 0.0;
        *ny = (roomcenter.y >= mid.y) ? 1.0 : -1.0;
        return true;
    }

    // 垂直边
    if (NearlyEqual(edge.a.x, edge.b.x))
    {
        Point right;
        right.x = mid.x + probe;
        right.y = mid.y;
        Point left;
        left.x = mid.x - probe;
        left.y = mid.y;

        bool inRight = IsPointInPolygon(right, problem_.contour);
        bool inLeft = IsPointInPolygon(left, problem_.contour);

        if (inRight != inLeft)
        {
            *nx = inRight ? 1.0 : -1.0;
            *ny = 0.0;
            return true;
        }

        // 兜底：用房间中心判断
        *nx = (roomcenter.x >= mid.x) ? 1.0 : -1.0;
        *ny = 0.0;
        return true;
    }
    return false;
}

std::vector<Solver::Candidate> Solver::GenerateWallCandidates(int item_index) const
{
    std::vector<Solver::Candidate> res;
    const Item& item = problem_.items[item_index];

    int n = static_cast<int>(problem_.contour.size());
    if (n < 2)
    {
        return res;
    }

    for (int angleIndex = 0; angleIndex < 2; angleIndex++)
    {
        int angle = (angleIndex == 0) ? 0 : 90;

        double w = RotatedSpanX(item, angle);
        double h = RotatedSpanY(item, angle);

        double sampleStep = ComputeAdaptiveStep(item, angle, true);

        for (int i = 0; i < n; i++)
        {
            Segment edge;
            edge.a = problem_.contour[i];
            edge.b = problem_.contour[(i + 1) % n];

            double nx = 0.0;
            double ny = 0.0;

            // 只处理水平/垂直边；并推断室内方向
            if (!InferInteriorNormal(edge, &nx, &ny))
            {
                continue;
            }

            // 水平边：中心 y 固定，x 沿边滑动
            if (NearlyEqual(edge.a.y, edge.b.y))
            {
                double y = edge.a.y + ny * (h / 2.0);

                double x1 = std::min(edge.a.x, edge.b.x);
                double x2 = std::max(edge.a.x, edge.b.x);

                std::vector<double> xs;
                AppendLineSamples(x1, x2, w / 2.0, sampleStep, xs);

                for (int k = 0; k < static_cast<int>(xs.size()); k++)
                {
                    Candidate cand;
                    cand.center.x = xs[k];
                    cand.center.y = y;
                    cand.angle = angle;
                    cand.body = MakeRectFromCenter(cand.center, w, h);
                    cand.wall_hugging = true;

                    if (BasicCandidateValid(cand))
                    {
                        AddCandidateUnique(res, cand);
                    }
                }
            }
            // 垂直边：中心 x 固定，y 沿边滑动
            else if (NearlyEqual(edge.a.x, edge.b.x))
            {
                double x = edge.a.x + nx * (w / 2.0);

                double y1 = std::min(edge.a.y, edge.b.y);
                double y2 = std::max(edge.a.y, edge.b.y);

                std::vector<double> ys;
                AppendLineSamples(y1, y2, h / 2.0, sampleStep, ys);

                for (int k = 0; k < static_cast<int>(ys.size()); k++)
                {
                    Candidate cand;
                    cand.center.x = x;
                    cand.center.y = ys[k];
                    cand.angle = angle;
                    cand.body = MakeRectFromCenter(cand.center, w, h);
                    cand.wall_hugging = true;

                    if (BasicCandidateValid(cand))
                    {
                        AddCandidateUnique(res, cand);
                    }
                }
            }
        }
    }
    return res;
}

std::vector<Solver::Candidate> Solver::GenerateInteriorCandidates(int item_index, double step) const
{
    std::vector<Solver::Candidate> res;
    const Item& item = problem_.items[item_index];

    for (int angleIndex = 0; angleIndex < 2; angleIndex++)
    {
        int angle = (angleIndex == 0) ? 0 : 90;

        double w = RotatedSpanX(item, angle);
        double h = RotatedSpanY(item, angle);

        double useStep = step;
        if (useStep <= EPS)
        {
            useStep = ComputeAdaptiveStep(item, angle, false);
        }

        double min_x = room_bbox_.x1 + w / 2.0;
        double max_x = room_bbox_.x2 - w / 2.0;
        double min_y = room_bbox_.y1 + h / 2.0;
        double max_y = room_bbox_.y2 - h / 2.0;

        if (min_x > max_x || min_y > max_y)
        {
            continue;
        }

        for (double x = min_x; x <= max_x + EPS; x += useStep)
        {
            for (double y = min_y; y <= max_y + EPS; y += useStep)
            {
                Candidate cand;
                cand.center.x = x;
                cand.center.y = y;
                cand.angle = angle;
                cand.body = MakeRectFromCenter(cand.center, w, h);
                cand.wall_hugging = false;

                if (BasicCandidateValid(cand))
                {
                    AddCandidateUnique(res, cand);
                }
            }
        }
        for (double x = min_x; x <= max_x + EPS; x += useStep)
        {
            Candidate cand;
            cand.center.x = x;
            cand.center.y = max_y;
            cand.angle = angle;
            cand.body = MakeRectFromCenter(cand.center, w, h);
            cand.wall_hugging = false;

            if (BasicCandidateValid(cand))
            {
                AddCandidateUnique(res, cand);
            }
        }

        for (double y = min_y; y <= max_y + EPS; y += useStep)
        {
            Candidate cand;
            cand.center.x = max_x;
            cand.center.y = y;
            cand.angle = angle;
            cand.body = MakeRectFromCenter(cand.center, w, h);
            cand.wall_hugging = false;

            if (BasicCandidateValid(cand))
            {
                AddCandidateUnique(res, cand);
            }
        }
        {
            Candidate cand;
            cand.center.x = max_x;
            cand.center.y = max_y;
            cand.angle = angle;
            cand.body = MakeRectFromCenter(cand.center, w, h);
            cand.wall_hugging = false;

            if (BasicCandidateValid(cand))
            {
                AddCandidateUnique(res, cand);
            }
        }
    }
    return res;
}

std::vector<int> Solver::BuildSearchOrder(const std::vector<std::vector<Candidate>>& all_candidates) const
{
    std::vector<int> order;
    for (int i = 0; i < problem_.items.size(); ++i)
    {
        order.push_back(i);
    }

    std::sort(order.begin(), order.end(), [this, &all_candidates](int a, int b)
        {
        // 优先级1：冰箱先放
        if (problem_.items[a].type == ItemType::FRIDGE && problem_.items[b].type != ItemType::FRIDGE)
        {
            return true;
        }
        if (problem_.items[b].type == ItemType::FRIDGE && problem_.items[a].type != ItemType::FRIDGE)
        {
            return false;
        }
        // 优先级2：候选数量少的先放（MRV - Minimum Remaining Values）
        if (all_candidates[a].size() != all_candidates[b].size())
        {
            return all_candidates[a].size() < all_candidates[b].size();
        }
        // 优先级3：大的家具先放
        double size_a = problem_.items[a].length * problem_.items[a].width;
        double size_b = problem_.items[b].length * problem_.items[b].width;
        if (!NearlyEqual(size_a, size_b))
        {
            return size_a > size_b;
        }
        return a < b;
    });

    return order;
}

bool Solver::DFS(int depth, const std::vector<int>& order, const std::vector<std::vector<Candidate>>& all_candidates, std::vector<Placement>& placed, Solution* out_solution) const
{
    if (depth == order.size())
    {
        out_solution->placements = placed;
        out_solution->feasible = true;
        return true;
    }
    int item_index = order[depth];
    for (int i = 0; i < all_candidates[item_index].size(); i++)
    {
        const Candidate& cand = all_candidates[item_index][i];
        Placement p = BuildPlacement(item_index, cand);
        if (!IsPlacementValid(p, placed))
        {
            continue;
        }
        placed.push_back(p);
        if (DFS(depth + 1, order, all_candidates, placed, out_solution))
        {
            return true;
        }
        placed.pop_back();
    }
    return false;
}

bool Solver::TrySolveWithCandidates(const std::vector<std::vector<Candidate>>& all_candidates, Solution* out_solution) const
{
    for (int i = 0; i < all_candidates.size(); i++)
    {
        if (all_candidates[i].empty())
        {
            return false;
        }
    }
    vector<int> order = BuildSearchOrder(all_candidates);
    vector<Placement> placed;
    return DFS(0, order, all_candidates, placed, out_solution);
}
