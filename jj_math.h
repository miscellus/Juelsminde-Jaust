#ifndef JJ_MATH_H
#define JJ_MATH_H

typedef struct Intersection_Points {
    bool are_intersecting;
    Vector2 intersection_points[2];
} Intersection_Points;


float sign_float(float v);

float abs_float(float v);

Vector2 Vector2Sign(Vector2 v);

Vector2 Vector2Abs(Vector2 v);

Vector2 Vector2NormalizeOrZero(Vector2 v);

Intersection_Points intersection_points_from_two_circles(Vector2 center1, float radius1, Vector2 center2, float radius2);

#endif
