#pragma once
#include "model.h"
#include <vector>

class Solver {
public:
    explicit Solver(const Problem& problem);

    Solution Solve() const;

private:
    struct Candidate
    {
        Point center;
        int angle = 0; // 0 / 90
        Rect body;
        bool wall_hugging = false;
    };

    double RotatedSpanX(const Item& item, int angle) const;
    double RotatedSpanY(const Item& item, int angle) const;

    Rect BuildDoorBlock() const;
    Rect BuildFridgeForbiddenZone(const Rect& body, int angle, const Item& item) const;

    Placement BuildPlacement(int item_index, const Solver::Candidate& c) const;

    bool BasicCandidateValid(const Solver::Candidate& c) const;
    bool IsPlacementValid(const Placement& p, const std::vector<Placement>& placed) const;

    void AddCandidateUnique(std::vector<Solver::Candidate>& out, const Solver::Candidate& c) const;
    std::vector<Solver::Candidate> GenerateWallCandidates(int item_index) const;
    std::vector<Solver::Candidate> GenerateInteriorCandidates(int item_index, double step) const;

    std::vector<int> BuildSearchOrder(
        const std::vector<std::vector<Solver::Candidate>>& all_candidates
    ) const;

    bool DFS(
        int depth,
        const std::vector<int>& order,
        const std::vector<std::vector<Solver::Candidate>>& all_candidates,
        std::vector<Placement>& placed,
        Solution* out_solution
    ) const;

    bool TrySolveWithCandidates(
        const std::vector<std::vector<Solver::Candidate>>& all_candidates,
        Solution* out_solution
    ) const;

    const Problem& problem_;
    Rect room_bbox_;
    Rect door_block_;
    double default_grid_step_ = 1.0;
};