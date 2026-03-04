#pragma once
#include "geometry.h"

#include <string>
#include <vector>

enum class DoorType {
    OUTWARD,
    INWARD
};

enum class ItemType {
    FRIDGE,
    SHELF,
    OVERSHELF,
    ICEMAKER
};

struct Door {
    Segment seg;
    DoorType type = DoorType::OUTWARD;
};

struct Item {
    std::string name;
    ItemType type = ItemType::SHELF;
    double length = 0.0; // angle=0 时沿 x 方向
    double width = 0.0;  // angle=0 时沿 y 方向
};

struct Placement {
    int item_index = -1;
    Point center;
    int angle = 0;   // 0 / 90
    Rect body;

    bool has_forbidden_zone = false;
    Rect forbidden_zone;
};

struct Problem {
    std::vector<Point> contour;
    Door door;
    std::vector<Item> items;
};

struct Solution {
    bool feasible = false;
    std::vector<Placement> placements;
};