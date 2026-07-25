/**
 * @file vec2.h
 * @brief 2D Vector math for movement and positioning.
 * * In this engine, every position and velocity is a Vec2. 
 * Think of 'x' as horizontal and 'y' as vertical coordinates.
 */

#ifndef ENGINE_CORE_VEC2_H
#define ENGINE_CORE_VEC2_H

typedef struct Vec2 {
    float x;
    float y;
} Vec2;


void vec2_add(struct Vec2 *out, const struct Vec2 *a, const struct Vec2 *b);
void vec2_sub(struct Vec2 *out, struct Vec2 *a, struct Vec2 *b);
void mulVec2(struct Vec2 *out, struct Vec2 *a, struct Vec2 *b);
float dotProduct(struct Vec2 *a, struct Vec2 *b);
float vec2Determinant(struct Vec2 *a, struct Vec2 *b);
float lengthVec2(struct Vec2 *a);
float vec2Rotation(struct Vec2 *a);

#endif
