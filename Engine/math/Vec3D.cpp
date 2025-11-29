#include <cmath>

#include "Vec3D.h"
#include "../Consts.h"

Vec3D::Vec3D(const Vec3D &vec) : x(vec.x), y(vec.y), z(vec.z){}

Vec3D::Vec3D(double x, double y, double z) : x(x), y(y), z(z){}

Vec3D::Vec3D(const Vec2D &vec, double z) : x(vec.x), y(vec.y), z(z){}

Vec3D Vec3D::operator-() const {
    return Vec3D(-x, -y, -z);
}

bool Vec3D::operator==(const Vec3D &vec) const {
    return (*this - vec).sqrAbs() <= Consts::EPS;
}

bool Vec3D::operator!=(const Vec3D &vec) const {
    return !(*this == vec);
}

// Operations with Vec3D
Vec3D Vec3D::operator+(const Vec3D &vec) const {
    return Vec3D(x + vec.x, y + vec.y, z + vec.z);
}

Vec3D Vec3D::operator-(const Vec3D &vec) const {
    return Vec3D(x - vec.x, y - vec.y, z - vec.z);
}

void Vec3D::operator+=(const Vec3D &vec) {
    x = x + vec.x;
    y = y + vec.y;
    z = z + vec.z;
}

void Vec3D::operator-=(const Vec3D &vec) {
    x = x - vec.x;
    y = y - vec.y;
    z = z - vec.z;
}

Vec3D Vec3D::operator*(double number) const {
    return Vec3D(x * number, y * number, z * number);
}

Vec3D Vec3D::operator*(const Vec3D &vec) const {
    return Vec3D(x * vec.x, y * vec.y, z * vec.z);
}

Vec3D Vec3D::operator/(double number) const {
    if (std::abs(number) > Consts::EPS) {
        return Vec3D(x / number, y / number, z / number);;
    }
    std::domain_error{"Vec3D::operator/(double number): division by zero"};
}

// Other useful methods
double Vec3D::sqrAbs() const {
    return x * x + y * y + z * z;
}

double Vec3D::abs() const {
    return std::sqrt(sqrAbs());
}

Vec3D Vec3D::normalized() const {
    double vecAbs = abs();
    if (vecAbs > Consts::EPS){
        return Vec3D(*this) / vecAbs;
    }
    return Vec3D(0);
}

double Vec3D::dot(const Vec3D &vec) const {
    return x * vec.x + y * vec.y + z * vec.z;
}

Vec3D Vec3D::cross(const Vec3D &vec) const {
    return Vec3D(y*vec.z-vec.y*z, 
                 z*vec.x-vec.z*x,
                 x*vec.y-vec.x*y);
}

Vec3D Vec3D::Random() {
    return Vec3D((double) rand() / RAND_MAX, (double) rand() / RAND_MAX, (double) rand() / RAND_MAX);
}

bool Vec3D::isNear(double a, double b) {
    return std::abs(a - b) < Consts::EPS;
}
