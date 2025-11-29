#include <cmath>

#include "Vec2D.h"
#include "../Consts.h"

Vec2D::Vec2D(const Vec2D &vec) : x(vec.x), y(vec.y){}

Vec2D::Vec2D(double x, double y) : x(x), y(y) {}

Vec2D Vec2D::operator-() const {
    return Vec2D(-x, -y);
}

bool Vec2D::operator==(const Vec2D &vec) const {
    Vec2D diff = *this - vec;
    return diff.sqrAbs() < Consts::EPS;
}

bool Vec2D::operator!=(const Vec2D &vec) const {
    return !(*this == vec);
}

Vec2D Vec2D::operator+(const Vec2D &vec) const {
    return Vec2D(x + vec.x, y + vec.y);
}

Vec2D Vec2D::operator-(const Vec2D &vec) const {
    return Vec2D(x - vec.x, y - vec.y);
}

Vec2D Vec2D::operator-(double num) const {
    return Vec2D(x - num, y - num);
}

void Vec2D::operator+=(const Vec2D &vec) {
    x = x + vec.x;
    y = y + vec.y;
}

void Vec2D::operator-=(const Vec2D &vec) {
    x = x - vec.x;
    y = y - vec.y;
}

Vec2D Vec2D::operator*(double number) const {
    return Vec2D(x * number, y * number);
}

Vec2D Vec2D::operator/(double number) const {
    if (std::abs(number) > Consts::EPS){
        return Vec2D(x / number, y / number);
    }
    std::domain_error{"Vec2D::operator/(double number): division by zero"};
}

// Other useful methods
double Vec2D::sqrAbs() const {
    return x * x + y * y;
}

double Vec2D::abs() const {
    return std::sqrt(sqrAbs());
}

Vec2D Vec2D::normalized() const {
    double vecAbs = abs();
    if (vecAbs > Consts::EPS){
        return Vec2D(*this) / vecAbs;
    }
    return Vec2D(0);
}

double Vec2D::dot(const Vec2D &vec) const {
    return x * vec.x + y * vec.y;
}

bool Vec2D::isNear(double a, double b) {
    return std::abs(a - b) < Consts::EPS;
}

double Vec2D::length() const {
    return sqrt(x*x + y*y);
}