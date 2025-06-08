typedef struct {
    const double mass;
    const double radius;
    const double defaultCharge;
    // Другие константные характеристики
} AtomTypeData;

AtomTypeData AtomType[] = {
    {1.008, 0.53, 0.0},   // Водород
    {12.011, 0.67, 0.0},  // Углерод
    {14.007, 0.56, 0.0},  // Азот
    {15.999, 0.48, 0.0}   // Кислород
};