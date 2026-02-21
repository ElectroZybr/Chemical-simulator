#ifndef VEC3D
#define VEC3D

#include "Vec2D.h"
#include <array>

class Vec3D final {
private:
    static bool isNear(double a, double b);

public:
    double x, y, z;

    Vec3D(const Vec3D &vec);

    explicit Vec3D(double x, double y = 0.0, double z = 0.0);

    Vec3D(const Vec2D &vec, double z = 0.0);

    [[nodiscard]] Vec3D operator-() const;

    // Boolean operations
    bool operator==(const Vec3D &vec) const;
    bool operator!=(const Vec3D &vec) const;

    [[nodiscard]] Vec3D operator+(const Vec3D &vec) const;
    [[nodiscard]] Vec3D operator-(const Vec3D &vec) const;
    void operator+=(const Vec3D &vec);
    void operator-=(const Vec3D &vec);

    [[nodiscard]] double dot(const Vec3D &vec) const; // Returns dot product
    [[nodiscard]] Vec3D cross(const Vec3D &vec) const; // Returns cross product

    // Operations with numbers
    [[nodiscard]] Vec3D operator*(double number) const;
    [[nodiscard]] Vec3D operator*(const Vec3D &vec) const;
    [[nodiscard]] Vec3D operator/(double number) const;

    // Other useful methods
    [[nodiscard]] double sqrAbs() const; // Returns squared vector length
    [[nodiscard]] double abs() const; // Returns vector length
    [[nodiscard]] Vec3D normalized() const; // Returns normalized vector without changing

    [[nodiscard]] double length() const;

    static Vec3D Random();
};


#endif
