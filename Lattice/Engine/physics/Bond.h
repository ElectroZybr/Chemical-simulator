#pragma once

#include <cstddef>
#include <cstdint>
#include <list>
#include <vector>

#include "Engine/physics/BondTable.h"

class AtomStorage;

class Bond {
private:
public:
    static BondTable bond_default_props;
    using List = std::list<Bond>;
    // Adjacency: для каждого атома i — список индексов атомов, с которыми i уже
    // связан. Снимает бутылочное горлышко формации связей O(N²)→O(N): прежде
    // dup-check в CreateBond линейно сканировал ВЕСЬ список связей — O(B) на
    // каждую из ~N кандидатных пар = O(N²) (на 15625 атомов было 1.58 c за один
    // вызов). Per-atom lookup делает проверку O(degree)≈O(1) (степень ограничена
    // валентностью) → суммарно O(N). Числа (75-121×): Benchmarks/RESULTS.md §D2.
    using Adjacency = std::vector<std::vector<uint32_t>>;

    static void ensureInitialized();
    // adjacency=nullptr → dup-check через линейный скан bonds (для редких
    // одиночных вызовов из UI). adjacency!=nullptr → O(degree) lookup + апдейт
    // адъяcent-структуры при успешном создании.
    static Bond* CreateBond(List& bonds, size_t aIndex, size_t bIndex, AtomStorage& atomStorage, Adjacency* adjacency = nullptr);
    static void BreakBond(List& bonds, Bond* bond, AtomStorage& atomStorage);
    static void angleForce(AtomStorage& atomStorage, size_t aIndex, size_t bIndex, size_t cIndex);

    Bond(size_t aIndex, size_t bIndex, AtomData::Type aType, AtomData::Type bType);

    void forceBond(AtomStorage& atomStorage, float dt);
    bool shouldBreak(const AtomStorage& atomStorage) const;
    void detach(AtomStorage& atomStorage);
    float MorseForce(float distanse);

    size_t aIndex;
    size_t bIndex;

    BondParams params;
};
