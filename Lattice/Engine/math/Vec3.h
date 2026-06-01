#pragma once

#include <cmath>
#include <concepts>
#include <numbers>

#include "Engine/Consts.h"
#include "Engine/math/Vec2.h"

template <Arithmetic T> class Vec3 final {
public:
    T x, y, z;

    constexpr Vec3(const Vec3&) = default;
    constexpr Vec3(T x = T{}, T y = T{}, T z = T{}) : x(x), y(y), z(z) {}

    template <Arithmetic U> explicit constexpr Vec3(const Vec2<U>& v, T z = T{}) : x(static_cast<T>(v.x)), y(static_cast<T>(v.y)), z(z) {}

    template <Vec2Like Other>
        requires requires(Other v) {
            { v.z } -> std::convertible_to<T>;
        }
    explicit constexpr Vec3(const Other& v) : x(static_cast<T>(v.x)), y(static_cast<T>(v.y)), z(static_cast<T>(v.z)) {}

    [[nodiscard]] constexpr Vec3 operator-() const { return {-x, -y, -z}; }

    [[nodiscard]] bool operator==(const Vec3& v) const { return (*this - v).sqrAbs() <= static_cast<T>(Consts::Epsilon); }
    [[nodiscard]] bool operator!=(const Vec3& v) const { return !(*this == v); }

    [[nodiscard]] constexpr Vec3 operator+(const Vec3& v) const { return {x + v.x, y + v.y, z + v.z}; }
    [[nodiscard]] constexpr Vec3 operator-(const Vec3& v) const { return {x - v.x, y - v.y, z - v.z}; }

    [[nodiscard]] constexpr Vec3 operator*(const Vec3& v) const { return {x * v.x, y * v.y, z * v.z}; }
    template <Arithmetic S>
    [[nodiscard]] constexpr Vec3 operator/(const Vec3<S>& v) const
        requires FloatingPoint<T>
    {
        return {x / static_cast<T>(v.x), y / static_cast<T>(v.y), z / static_cast<T>(v.z)};
    }

    constexpr Vec3& operator+=(const Vec3& v) {
        x += v.x;
        y += v.y;
        z += v.z;
        return *this;
    }
    constexpr Vec3& operator-=(const Vec3& v) {
        x -= v.x;
        y -= v.y;
        z -= v.z;
        return *this;
    }

    template <Arithmetic S> constexpr Vec3& operator*=(S s) {
        const auto scalar = static_cast<T>(s);
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }

    template <Arithmetic S> [[nodiscard]] constexpr Vec3 operator+(S s) const {
        return {x + static_cast<T>(s), y + static_cast<T>(s), z + static_cast<T>(s)};
    }
    template <Arithmetic S> [[nodiscard]] constexpr Vec3 operator-(S s) const {
        return {x - static_cast<T>(s), y - static_cast<T>(s), z - static_cast<T>(s)};
    }
    template <Arithmetic S> [[nodiscard]] constexpr Vec3 operator*(S s) const {
        return {x * static_cast<T>(s), y * static_cast<T>(s), z * static_cast<T>(s)};
    }

    template <Arithmetic S>
    [[nodiscard]] constexpr Vec3 operator/(S s) const
        requires FloatingPoint<T>
    {
        const auto scalar = static_cast<T>(s);
        return {x / scalar, y / scalar, z / scalar};
    }

    template <Arithmetic S>
    [[nodiscard]] constexpr Vec3 operator/(S s) const
        requires std::integral<T>
    {
        const auto scalar = static_cast<T>(s);
        return {x / scalar, y / scalar, z / scalar};
    }

    template <Arithmetic S> [[nodiscard]] friend constexpr Vec3 operator*(S s, const Vec3& v) { return v * s; }

    [[nodiscard]] constexpr T sqrAbs() const { return x * x + y * y + z * z; }

    [[nodiscard]] T abs() const
        requires FloatingPoint<T>
    {
        return std::sqrt(sqrAbs());
    }

    [[nodiscard]] constexpr T dot(const Vec3& v) const { return x * v.x + y * v.y + z * v.z; }

    [[nodiscard]] constexpr Vec3 cross(const Vec3& v) const { return {y * v.z - v.y * z, z * v.x - v.z * x, x * v.y - v.x * y}; }

    [[nodiscard]] Vec3 normalized() const
        requires FloatingPoint<T>
    {
        const T len = abs();
        return len > static_cast<T>(Consts::Epsilon) ? *this / len : Vec3{};
    }

    [[nodiscard]] static Vec3 Random()
        requires FloatingPoint<T>
    {
        const T phi = T{2} * std::numbers::pi_v<T> * (static_cast<T>(rand()) / static_cast<T>(RAND_MAX));
        const T cos_theta = T{2} * (static_cast<T>(rand()) / static_cast<T>(RAND_MAX)) - T{1};
        const T sin_theta = std::sqrt(T{1} - cos_theta * cos_theta);
        return {sin_theta * std::cos(phi), sin_theta * std::sin(phi), cos_theta};
    }

    [[nodiscard]] constexpr Vec2<T> xy() const { return {x, y}; }
};

using Vec3f = Vec3<float>;
using Vec3d = Vec3<double>;
using Vec3i = Vec3<int>;
using Vec3u = Vec3<unsigned>;
