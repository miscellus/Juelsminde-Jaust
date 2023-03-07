#include "jj_math.h"
#include <raymath.h>


float abs_float(float v) {
    return v >= 0 ? v : -v;
}

Vector2 Vector2Abs(Vector2 v) {
    Vector2 result;

    result.x = abs_float(v.x); 
    result.y = abs_float(v.y); 

    return result;
}

Vector2 Vector2NormalizeOrZero(Vector2 v) {
    float length = Vector2Length(v);
    
    if (length == 0.0f) return v;

    return Vector2Scale(v, 1.0f/length);
}

Intersection_Points intersection_points_from_two_circles(Circle c1, Circle c2) {

    Intersection_Points result = {.are_intersecting = false};

    Vector2 delta = Vector2Subtract(c2.center, c1.center);

    float radii_sum = c2.radius + c1.radius;
    float radii_difference = c2.radius - c1.radius;

    float squared_distance = Vector2LengthSqr(delta);

    if (squared_distance > radii_sum*radii_sum) {
        // no solutions, the circles are separate
        return result;
    }
    else if (squared_distance <= radii_difference*radii_difference) {
        // no solutions, one circle is contained within the other,
        // or circles are identical and are overlapping completely (infinite solutions)
        return result;
    }
    
    float inv_distance = 1.0f/sqrtf(squared_distance);

    float radius1_squared = c1.radius*c1.radius;
    float radius2_squared = c2.radius*c2.radius;

    float a = (radius1_squared - radius2_squared + squared_distance)*( 0.5f * inv_distance );
    float h = sqrtf(radius1_squared - a*a);

    Vector2 m = Vector2Add(c1.center, Vector2Scale(delta, a*inv_distance));

    Vector2 delta_scaled = Vector2Scale(delta, h*inv_distance);

    result.are_intersecting = true;
    result.intersection_points[0] = (Vector2){m.x + delta_scaled.y, m.y - delta_scaled.x};
    result.intersection_points[1] = (Vector2){m.x - delta_scaled.y, m.y + delta_scaled.x};

    return result;
}
