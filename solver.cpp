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

    cout << "[调试] 房间边界框: (" << room_bbox_.x1 << ", " << room_bbox_.y1
         << ") -> (" << room_bbox_.x2 << ", " << room_bbox_.y2 << ")" << endl;
    cout << "[调试] 门区域: (" << door_block_.x1 << ", " << door_block_.y1
         << ") -> (" << door_block_.x2 << ", " << door_block_.y2 << ")" << endl;
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

            vector<Candidate> extra = GenerateInteriorCandidates(i, default_grid_step_);

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
}

Rect Solver::BuildFridgeForbiddenZone(const Rect& body, int angle, const Item& item) const
{
    double forbidden = item.width;
    Rect res;
    if (angle == 0)
    {
        res.x1 = body.x1;
        res.x2 = body.x2;
        res.y1 = body.y2;
        res.y2 = body.y2 + forbidden;
    }
    else if (angle == 90)
    {
        res.x1 = body.x1;
        res.x2 = body.x2 + forbidden;
        res.y1 = body.y2;
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
        cout << "  [调试] 候选 (" << c.center.x << ", " << c.center.y << ") 不在轮廓内" << endl;
        return false;
    }
    // 家具不能与门区域重叠（包括边接触）
    if (!IsZeroRect(door_block_))
    {
        cout << "  [调试] 检查门区域: body=(" << c.body.x1 << "," << c.body.y1 << ")->(" << c.body.x2 << "," << c.body.y2 << ") "
             << "door=(" << door_block_.x1 << "," << door_block_.y1 << ")->(" << door_block_.x2 << "," << door_block_.y2 << ")" << endl;
        if (IsRectOverlap(c.body, door_block_, false))
        {
            cout << "  [调试] 候选 (" << c.center.x << ", " << c.center.y << ") 与门区域重叠" << endl;
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

std::vector<Solver::Candidate> Solver::GenerateWallCandidates(int item_index) const
{
    std::vector<Solver::Candidate> res;
    const Item& item = problem_.items[item_index];

    for (int angleIndex = 0; angleIndex < 2; angleIndex++)
    {
        int angle = 0;
        if (angleIndex == 0)
        {
            angle = 0;
        }
        else
        {
            angle = 90;
        }

        double w = RotatedSpanX(item, angle);
        double h = RotatedSpanY(item, angle);

        double min_x = room_bbox_.x1 + w / 2.0;
        double max_x = room_bbox_.x2 - w / 2.0;
        double min_y = room_bbox_.y1 + h / 2.0;
        double max_y = room_bbox_.y2 - h / 2.0;

        if (min_x > max_x || min_y > max_y)
        {
            continue;
        }

        std::vector<double> x_positions = { min_x, (min_x + max_x) / 2.0, max_x };
        std::vector<double> y_positions = { min_y, (min_y + max_y) / 2.0, max_y };

        // 贴下墙、贴上墙
        for (int i = 0; i < static_cast<int>(x_positions.size()); i++)
        {
            double x = x_positions[i];

            Candidate cand;

            // 贴下墙
            cand.center.x = x;
            cand.center.y = min_y;
            cand.angle = angle;
            cand.body = MakeRectFromCenter(cand.center, w, h);
            cand.wall_hugging = true;
            if (BasicCandidateValid(cand))
            {
                AddCandidateUnique(res, cand);
            }

            // 贴上墙
            cand.center.x = x;
            cand.center.y = max_y;
            cand.angle = angle;
            cand.body = MakeRectFromCenter(cand.center, w, h);
            cand.wall_hugging = true;
            if (BasicCandidateValid(cand))
            {
                AddCandidateUnique(res, cand);
            }
        }

        // 贴左墙、贴右墙
        for (int i = 0; i < static_cast<int>(y_positions.size()); i++)
        {
            double y = y_positions[i];

            Candidate cand;

            // 贴左墙
            cand.center.x = min_x;
            cand.center.y = y;
            cand.angle = angle;
            cand.body = MakeRectFromCenter(cand.center, w, h);
            cand.wall_hugging = true;
            if (BasicCandidateValid(cand))
            {
                AddCandidateUnique(res, cand);
            }

            // 贴右墙
            cand.center.x = max_x;
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
    return res;
}

std::vector<Solver::Candidate> Solver::GenerateInteriorCandidates(int item_index, double step) const
{
    std::vector<Solver::Candidate> res;
    const Item& item = problem_.items[item_index];
    if (step <= EPS)
    {
        step = default_grid_step_;
    }
    for (int angleIndex = 0; angleIndex < 2; angleIndex++)
    {
        int angle = (angleIndex == 0) ? 0 : 90;

        double w = RotatedSpanX(item, angle);
        double h = RotatedSpanY(item, angle);

        double min_x = room_bbox_.x1 + w / 2.0;
        double max_x = room_bbox_.x2 - w / 2.0;
        double min_y = room_bbox_.y1 + h / 2.0;
        double max_y = room_bbox_.y2 - h / 2.0;

        if (min_x > max_x || min_y > max_y)
        {
            continue;
        }
        for (double x = min_x; x <= max_x + EPS; x += step)
        {
            for (double y = min_y; y <= max_y + EPS; y += step)
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
