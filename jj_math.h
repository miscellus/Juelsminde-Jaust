#ifndef JJ_MATH_H
#define JJ_MATH_H

#define INV_SQRT_TWO 0.7071067811865475f

typedef struct Intersection_Points {
    bool are_intersecting;
    Vector2 intersection_points[2];
} Intersection_Points;

typedef struct Circle {
    Vector2 center;
    float radius;
} Circle;


float sign_float(float v);

float abs_float(float v);

Vector2 Vector2Sign(Vector2 v);

Vector2 Vector2Abs(Vector2 v);

Vector2 Vector2NormalizeOrZero(Vector2 v);

Intersection_Points intersection_points_from_two_circles(Circle c1, Circle c2);

#endif
