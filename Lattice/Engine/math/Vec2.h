#pragma once
#include <cmath>
#include <concepts>
#include <numbers>
#include <type_traits>
#include <stdlib.h>

#include "Engine/Consts.h"

template <typename T>
concept Arithmetic = std::is_arithmetic_v<T>;

template <typename T>
concept FloatingPoint = std::floating_point<T>;

template <typename T>
concept Vec2Like = requires(T v) {
    requires Arithmetic<decltype(v.x)>;
    requires Arithmetic<decltype(v.y)>;
};

template <Arithmetic T> class Vec2 {
public:
    T x, y;

    constexpr Vec2(const Vec2&) = default;
    constexpr Vec2(T x = T{}, T y = T{}) : x(x), y(y) {}

    template <Vec2Like Other> explicit constexpr Vec2(const Other& v) : x(static_cast<T>(v.x)), y(static_cast<T>(v.y)) {}

    [[nodiscard]] constexpr Vec2 operator-() const { return {-x, -y}; }

    [[nodiscard]] bool operator==(const Vec2& v) const { return (*this - v).sqrAbs() < static_cast<T>(Consts::Epsilon); }
    [[nodiscard]] bool operator!=(const Vec2& v) const { return !(*this == v); }

    [[nodiscard]] constexpr Vec2 operator+(const Vec2& v) const { return {x + v.x, y + v.y}; }
    [[nodiscard]] constexpr Vec2 operator-(const Vec2& v) const { return {x - v.x, y - v.y}; }
    [[nodiscard]] constexpr Vec2 operator*(const Vec2& v) const { return {x * v.x, y * v.y}; }
    template <Arithmetic S>
    [[nodiscard]] constexpr Vec2 operator/(const Vec2<S>& v) const
        requires FloatingPoint<T>
    {
        return {x / static_cast<T>(v.x), y / static_cast<T>(v.y)};
    }

    constexpr Vec2& operator+=(const Vec2& v) {
        x += v.x;
        y += v.y;
        return *this;
    }
    constexpr Vec2& operator-=(const Vec2& v) {
        x -= v.x;
        y -= v.y;
        return *this;
    }

    template <Arithmetic S> constexpr Vec2& operator*=(S s) {
        const auto scalar = static_cast<T>(s);
        x *= scalar;
        y *= scalar;
        return *this;
    }

    template <Arithmetic S> [[nodiscard]] constexpr Vec2 operator+(S s) const { return {x + static_cast<T>(s), y + static_cast<T>(s)}; }
    template <Arithmetic S> [[nodiscard]] constexpr Vec2 operator-(S s) const { return {x - static_cast<T>(s), y - static_cast<T>(s)}; }
    template <Arithmetic S> [[nodiscard]] constexpr Vec2 operator*(S s) const { return {x * static_cast<T>(s), y * static_cast<T>(s)}; }
    template <Arithmetic S>
    [[nodiscard]] constexpr Vec2 operator/(S s) const
        requires FloatingPoint<T>
    {
        const auto scalar = static_cast<T>(s);
        return {x / scalar, y / scalar};
    }
    template <Arithmetic S>
    [[nodiscard]] constexpr Vec2 operator/(S s) const
        requires std::integral<T>
    {
        const auto scalar = static_cast<T>(s);
        return {x / scalar, y / scalar};
    }

    template <Arithmetic S> [[nodiscard]] friend constexpr Vec2 operator*(S s, const Vec2& v) { return v * s; }

    [[nodiscard]] constexpr T sqrAbs() const { return x * x + y * y; }
    [[nodiscard]] T abs() const
        requires FloatingPoint<T>
    {
        return std::sqrt(sqrAbs());
    }

    [[nodiscard]] constexpr T dot(const Vec2& v) const { return x * v.x + y * v.y; }

    [[nodiscard]] Vec2 normalized() const {
        const T len = abs();
        return len > static_cast<T>(Consts::Epsilon) ? *this / len : Vec2{};
    }

    [[nodiscard]] static Vec2 Random()
        requires FloatingPoint<T>
    {
        const T phi = T{2} * std::numbers::pi_v<T> * (static_cast<T>(rand()) / static_cast<T>(RAND_MAX));
        return {std::cos(phi), std::sin(phi)};
    }
};

using Vec2f = Vec2<float>;
using Vec2d = Vec2<double>;
using Vec2i = Vec2<int>;
using Vec2u = Vec2<unsigned>;