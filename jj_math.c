#include "jj_math.h"
#include <raymath.h>


float sign_float(float v) {
    return v > 0 ? 1.0f : v < 0 ? -1.0f : 0.0f;
}

float abs_float(float v) {
    return v >= 0 ? v : -v;
}

Vector2 Vector2Sign(Vector2 v) {
    Vector2 result;

    result.x = sign_float(v.x); 
    result.y = sign_float(v.y); 

    return result;
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

Intersection_Points intersection_points_from_two_circles(Vector2 center1, float radius1, Vector2 center2, float radius2) {

    Intersection_Points result = {.are_intersecting = false};

    Vector2 delta = Vector2Subtract(center2, center1);

    float radii_sum = radius2 + radius1;
    float radii_difference = radius2 - radius1;

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

    float radius1_squared = radius1*radius1;

    float a = (radius1_squared - radius2*radius2 + squared_distance)*( 0.5f * inv_distance );
    float h = sqrtf(radius1_squared - a*a);

    Vector2 m = Vector2Add(center1, Vector2Scale(delta, a*inv_distance));

    Vector2 delta_scaled = Vector2Scale(delta, h*inv_distance);

    result.are_intersecting = true;
    result.intersection_points[0] = (Vector2){m.x + delta_scaled.y, m.y - delta_scaled.x};
    result.intersection_points[1] = (Vector2){m.x - delta_scaled.y, m.y + delta_scaled.x};

    return result;
}
