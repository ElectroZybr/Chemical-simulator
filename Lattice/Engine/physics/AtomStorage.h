#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "Engine/math/Vec3.h"
#include "Engine/physics/AtomData.h"

// Хранилище атомов в раскладке SoA (structure-of-arrays): все поля
// (x, y, z, vx..vz, fx..fz, prev-силы, pe, invMass, charge) лежат в ОДНОМ
// непрерывном буфере floatData_ блоками по capacity_ элементов, а
// x_/y_/.../charge_ — raw-указатели на начала блоков (их наводит rebind).
// Зачем: обход одного поля по всем атомам (напр. все x подряд) идёт линейно
// по памяти — дружелюбно к кэшу и автовекторизации горячих циклов сил и
// интегратора. Инвариант раскладки: блок поля f начинается с
// floatData_[f * capacity_]; при росте capacity_ буфер переаллоцируется
// целиком и указатели наводятся заново.
class AtomStorage {
    enum class Field : size_t { X, Y, Z, Vx, Vy, Vz, Fx, Fy, Fz, Pfx, Pfy, Pfz, Pe, InvMass, Charge, Count };

    static constexpr size_t kFieldCount = static_cast<size_t>(Field::Count);

    std::array<float**, kFieldCount> fieldPtrs() {
        return {&x_, &y_, &z_, &vx_, &vy_, &vz_, &fx_, &fy_, &fz_, &pfx_, &pfy_, &pfz_, &pe_, &invMass_, &charge_};
    }

    size_t count_ = 0;
    size_t capacity_ = 0;
    size_t mobileCount_ = 0;

    std::vector<float> floatData_;

    float* x_ = nullptr;
    float* y_ = nullptr;
    float* z_ = nullptr;
    float* vx_ = nullptr;
    float* vy_ = nullptr;
    float* vz_ = nullptr;
    float* fx_ = nullptr;
    float* fy_ = nullptr;
    float* fz_ = nullptr;
    float* pfx_ = nullptr;
    float* pfy_ = nullptr;
    float* pfz_ = nullptr;
    float* pe_ = nullptr;
    float* invMass_ = nullptr;
    float* charge_ = nullptr;

    std::vector<AtomData::Type> atomType_;
    std::vector<uint8_t> valence_;

    // Обновляет все raw pointer члены по текущему floatData_ и capacity_.
    void rebind() {
        auto ptrs = fieldPtrs();
        if (capacity_ == 0 || floatData_.empty()) {
            for (float** p : ptrs) {
                *p = nullptr;
            }
            return;
        }
        float* base = floatData_.data();
        for (size_t i = 0; i < kFieldCount; ++i) {
            *ptrs[i] = base + i * capacity_;
        }
    }

    void ensureCapacity(size_t required) {
        if (required <= capacity_) {
            return;
        }

        // Геометрический рост (×1.5): добавление атомов по одному даёт
        // амортизированный O(1) на атом вместо O(n) реаллокаций; +1 уводит
        // с нуля.
        size_t newCap = (capacity_ == 0) ? required : capacity_;
        while (newCap < required) {
            newCap = newCap * 3 / 2 + 1;
        }

        std::vector<float> newData(kFieldCount * newCap, 0.f);

        if (count_ > 0) {
            auto ptrs = fieldPtrs();
            for (size_t i = 0; i < kFieldCount; ++i) {
                std::copy_n(*ptrs[i], count_, newData.data() + i * newCap);
            }
        }

        floatData_ = std::move(newData);
        capacity_ = newCap;
        rebind();
    }

    void resize(size_t count) {
        ensureCapacity(count);
        count_ = count;
        atomType_.resize(count);
        valence_.resize(count);
    }

public:
    AtomStorage() = default;
    AtomStorage(const AtomStorage&) = delete;
    AtomStorage& operator=(const AtomStorage&) = delete;

    AtomStorage(AtomStorage&& other) noexcept
        : count_(other.count_), capacity_(other.capacity_), mobileCount_(other.mobileCount_), floatData_(std::move(other.floatData_)),
          atomType_(std::move(other.atomType_)), valence_(std::move(other.valence_)) {
        rebind();
        other.count_ = other.capacity_ = other.mobileCount_ = 0;
        other.rebind();
    }

    AtomStorage& operator=(AtomStorage&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        count_ = other.count_;
        capacity_ = other.capacity_;
        mobileCount_ = other.mobileCount_;
        floatData_ = std::move(other.floatData_);
        atomType_ = std::move(other.atomType_);
        valence_ = std::move(other.valence_);
        rebind();
        other.count_ = other.capacity_ = other.mobileCount_ = 0;
        other.rebind();
        return *this;
    }

    // Фиксированные атомы должны быть в конце массива (индексы [mobileCount, count)).
    void init(size_t count, size_t mobileCount, std::span<const float> x, std::span<const float> y, std::span<const float> z,
              std::span<const float> vx, std::span<const float> vy, std::span<const float> vz, std::span<const AtomData::Type> atomType,
              std::span<const float> charge) {
        clear();
        resize(count);
        mobileCount_ = mobileCount;

        std::copy_n(x.data(), count, x_);
        std::copy_n(y.data(), count, y_);
        std::copy_n(z.data(), count, z_);
        std::copy_n(vx.data(), count, vx_);
        std::copy_n(vy.data(), count, vy_);
        std::copy_n(vz.data(), count, vz_);
        std::copy_n(charge.data(), count, charge_);
        std::copy_n(atomType.data(), count, atomType_.data());

        std::fill_n(fx_, count, 0.f);
        std::fill_n(fy_, count, 0.f);
        std::fill_n(fz_, count, 0.f);
        std::fill_n(pfx_, count, 0.f);
        std::fill_n(pfy_, count, 0.f);
        std::fill_n(pfz_, count, 0.f);
        std::fill_n(pe_, count, 0.f);

        for (size_t i = 0; i < count; ++i) {
            const auto& props = AtomData::getProps(atomType_[i]);
            invMass_[i] = 1.f / props.mass;
            valence_[i] = props.maxValence;
        }
    }

    void clear() {
        count_ = mobileCount_ = capacity_ = 0;
        floatData_.clear();
        floatData_.shrink_to_fit();
        atomType_.clear();
        atomType_.shrink_to_fit();
        valence_.clear();
        valence_.shrink_to_fit();
        rebind(); // обнуляем указатели, чтобы не держать висячие
    }

    void reserve(size_t count) {
        ensureCapacity(count);
        atomType_.reserve(count);
        valence_.reserve(count);
    }

    // fixed=true создаёт декоративный атом: он попадает в конец массива (в диапазон
    // [mobileCount, count)) и в дальнейшем не участвует в физике.
    //   - Integrator (Verlet/KDK) итерирует только [0, mobileCount) — атом не двигается.
    //   - ForceField::computePairInteractions ходит до mobileCount как центр пары.
    //   - NeighborList — half-list (j < i), так что для mobile i < mobileCount соседи
    //     j < mobileCount тоже всегда mobile. Mobile-fixed пары в NL не попадают.
    // Из этого следует: fixed-атомы не оказывают LJ/Coulomb-сил на мобильные. Если
    // понадобится семантика anchor (fixed влияет на mobile, но сам не двигается) —
    // это отдельная фича, требующая правки NL build и force loop.
    void addAtom(const Vec3f& coords, const Vec3f& speed, AtomData::Type type, bool fixed = false) {
        ensureCapacity(count_ + 1);

        x_[count_] = static_cast<float>(coords.x);
        y_[count_] = static_cast<float>(coords.y);
        z_[count_] = static_cast<float>(coords.z);

        vx_[count_] = static_cast<float>(speed.x);
        vy_[count_] = static_cast<float>(speed.y);
        vz_[count_] = static_cast<float>(speed.z);

        fx_[count_] = fy_[count_] = fz_[count_] = 0.f;
        pfx_[count_] = pfy_[count_] = pfz_[count_] = 0.f;
        pe_[count_] = 0.f;

        const auto& props = AtomData::getProps(type);
        invMass_[count_] = 1.f / props.mass;
        charge_[count_] = 0.f;
        if (type == AtomData::Type::Cl) {
            charge_[count_] = -1.f;
        }
        else if (type == AtomData::Type::Na) {
            charge_[count_] = 1.f;
        }

        atomType_.emplace_back(type);
        valence_.emplace_back(props.maxValence);
        ++count_;

        if (!fixed) {
            // Сохраняем инвариант: мобильные атомы идут первыми
            swapAtoms(count_ - 1, mobileCount_);
            ++mobileCount_;
        }
    }

    void removeAtom(size_t index) {
        if (index >= count_) {
            return;
        }

        const size_t last = count_ - 1;
        if (index < mobileCount_) {
            swapAtoms(index, mobileCount_ - 1);
            --mobileCount_;
            swapAtoms(mobileCount_, last);
        }
        else {
            if (index != last) {
                swapAtoms(index, last);
            }
        }

        atomType_.pop_back();
        valence_.pop_back();
        --count_;
    }

    void swapAtoms(size_t a, size_t b) {
        if (a >= count_ || b >= count_ || a == b) {
            return;
        }

        std::swap(x_[a], x_[b]);
        std::swap(y_[a], y_[b]);
        std::swap(z_[a], z_[b]);
        std::swap(vx_[a], vx_[b]);
        std::swap(vy_[a], vy_[b]);
        std::swap(vz_[a], vz_[b]);
        std::swap(fx_[a], fx_[b]);
        std::swap(fy_[a], fy_[b]);
        std::swap(fz_[a], fz_[b]);
        std::swap(pfx_[a], pfx_[b]);
        std::swap(pfy_[a], pfy_[b]);
        std::swap(pfz_[a], pfz_[b]);
        std::swap(pe_[a], pe_[b]);
        std::swap(invMass_[a], invMass_[b]);
        std::swap(charge_[a], charge_[b]);
        std::swap(atomType_[a], atomType_[b]);
        std::swap(valence_[a], valence_[b]);
    }

    void swapPrevCurrentForces() {
        std::swap(pfx_, fx_);
        std::swap(pfy_, fy_);
        std::swap(pfz_, fz_);
    }

    size_t size() const { return count_; }
    size_t mobileCount() const { return mobileCount_; }
    bool empty() const { return count_ == 0; }
    bool isAtomFixed(size_t i) const { return i >= mobileCount_; }

    size_t memoryBytes() const {
        return floatData_.capacity() * sizeof(float) + atomType_.capacity() * sizeof(AtomData::Type) +
               valence_.capacity() * sizeof(uint8_t);
    }

    float& posX(size_t i) { return x_[i]; }
    float& posY(size_t i) { return y_[i]; }
    float& posZ(size_t i) { return z_[i]; }
    const float& posX(size_t i) const { return x_[i]; }
    const float& posY(size_t i) const { return y_[i]; }
    const float& posZ(size_t i) const { return z_[i]; }

    float& velX(size_t i) { return vx_[i]; }
    float& velY(size_t i) { return vy_[i]; }
    float& velZ(size_t i) { return vz_[i]; }
    const float& velX(size_t i) const { return vx_[i]; }
    const float& velY(size_t i) const { return vy_[i]; }
    const float& velZ(size_t i) const { return vz_[i]; }

    float& forceX(size_t i) { return fx_[i]; }
    float& forceY(size_t i) { return fy_[i]; }
    float& forceZ(size_t i) { return fz_[i]; }
    const float& forceX(size_t i) const { return fx_[i]; }
    const float& forceY(size_t i) const { return fy_[i]; }
    const float& forceZ(size_t i) const { return fz_[i]; }

    float& prevForceX(size_t i) { return pfx_[i]; }
    float& prevForceY(size_t i) { return pfy_[i]; }
    float& prevForceZ(size_t i) { return pfz_[i]; }
    const float& prevForceX(size_t i) const { return pfx_[i]; }
    const float& prevForceY(size_t i) const { return pfy_[i]; }
    const float& prevForceZ(size_t i) const { return pfz_[i]; }

    float& invMass(size_t i) { return invMass_[i]; }
    const float& invMass(size_t i) const { return invMass_[i]; }

    float& charge(size_t i) { return charge_[i]; }
    const float& charge(size_t i) const { return charge_[i]; }

    float& energy(size_t i) { return pe_[i]; }
    const float& energy(size_t i) const { return pe_[i]; }

    AtomData::Type& type(size_t i) { return atomType_[i]; }
    const AtomData::Type& type(size_t i) const { return atomType_[i]; }

    uint8_t& valenceCount(size_t i) { return valence_[i]; }
    const uint8_t& valenceCount(size_t i) const { return valence_[i]; }

    Vec3f pos(size_t i) const { return {x_[i], y_[i], z_[i]}; }
    Vec3f vel(size_t i) const { return {vx_[i], vy_[i], vz_[i]}; }
    Vec3f force(size_t i) const { return {fx_[i], fy_[i], fz_[i]}; }
    Vec3f prevForce(size_t i) const { return {pfx_[i], pfy_[i], pfz_[i]}; }

    void setPos(size_t i, const Vec3f& v) {
        x_[i] = static_cast<float>(v.x);
        y_[i] = static_cast<float>(v.y);
        z_[i] = static_cast<float>(v.z);
    }
    void setVel(size_t i, const Vec3f& v) {
        vx_[i] = static_cast<float>(v.x);
        vy_[i] = static_cast<float>(v.y);
        vz_[i] = static_cast<float>(v.z);
    }
    void setForce(size_t i, const Vec3f& v) {
        fx_[i] = static_cast<float>(v.x);
        fy_[i] = static_cast<float>(v.y);
        fz_[i] = static_cast<float>(v.z);
    }
    void setPrevForce(size_t i, const Vec3f& v) {
        pfx_[i] = static_cast<float>(v.x);
        pfy_[i] = static_cast<float>(v.y);
        pfz_[i] = static_cast<float>(v.z);
    }

    float* xData() { return x_; }
    const float* xData() const { return x_; }
    float* yData() { return y_; }
    const float* yData() const { return y_; }
    float* zData() { return z_; }
    const float* zData() const { return z_; }
    float* vxData() { return vx_; }
    const float* vxData() const { return vx_; }
    float* vyData() { return vy_; }
    const float* vyData() const { return vy_; }
    float* vzData() { return vz_; }
    const float* vzData() const { return vz_; }
    float* fxData() { return fx_; }
    float* fyData() { return fy_; }
    float* fzData() { return fz_; }
    float* pfxData() { return pfx_; }
    float* pfyData() { return pfy_; }
    float* pfzData() { return pfz_; }
    float* energyData() { return pe_; }
    float* invMassData() { return invMass_; }
    float* chargeData() { return charge_; }
    const float* chargeData() const { return charge_; }

    AtomData::Type* atomTypeData() { return atomType_.data(); }
    const AtomData::Type* atomTypeData() const { return atomType_.data(); }

    std::span<const float> floatDataSpan() const { return {floatData_.data(), floatData_.size()}; }

    std::span<float> xDataSpan() const { return {x_, count_}; }
    std::span<float> yDataSpan() const { return {y_, count_}; }
    std::span<float> zDataSpan() const { return {z_, count_}; }
    std::span<float> vxDataSpan() const { return {vx_, count_}; }
    std::span<float> vyDataSpan() const { return {vy_, count_}; }
    std::span<float> vzDataSpan() const { return {vz_, count_}; }
    std::span<float> chargeDataSpan() const { return {charge_, count_}; }

    std::span<const AtomData::Type> atomTypeDataSpan() const { return {atomType_.data(), atomType_.size()}; }
    std::span<const uint8_t> valenceDataSpan() const { return {valence_.data(), valence_.size()}; }

    void setFixed(size_t i, bool fixed) {
        if (fixed) {
            if (i >= mobileCount_) {
                return;
            }
            --mobileCount_;
            swapAtoms(i, mobileCount_); // последний мобильный встаёт на место i
        }
        else {
            if (i < mobileCount_) {
                return;
            }
            swapAtoms(i, mobileCount_); // первый фиксированный встаёт на место i
            ++mobileCount_;
        }
    }
};
