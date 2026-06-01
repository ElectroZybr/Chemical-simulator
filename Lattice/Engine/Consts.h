#pragma once

namespace Consts {
    inline constexpr float Epsilon = 1e-6f;
}

namespace Units {
    // время
    inline constexpr float kTimeUnitToFs = 10.1805f;    // константа для перевода Dt -> Fs (1 Dt = 10.1805 fs)
    inline constexpr float kTimeUnitToNs = kTimeUnitToFs * 1e-6f; // константа для перевода Dt -> Ns (1 Dt = 0.0000101805 ns)
	inline constexpr float kTimeUnitToS = kTimeUnitToFs * 1e-15f;
	inline constexpr float kTimeUnitToHour = kTimeUnitToS / 3600.f;

    // единицы измерения
    inline constexpr float AngstromToNm = 0.1f;  // константа для перевода ангстрем в нанометры
    inline constexpr float NmToAngstrom = 10.0f; // константа для перевода нанометров в ангстремы

    // энергия
    inline constexpr float kEnergyUnitToEv = 1.0f;       // константа для перевода единиц энергии в электрон-вольты
    inline constexpr float kEvToJ = 1.602176634e-19f;    // константа для перевода электрон-вольт в джоули
    inline constexpr float kEvToPJ = kEvToJ * 1e12f;     // константа для перевода электрон-вольт в пико джоули

    // температура
    inline constexpr float kboltzmann = 8.617333262e-5f; // постоянная Больцмана в электрон-вольтах на кельвин

    // скорость
    inline constexpr float SpeedUnitToMps = 1e-10f / kTimeUnitToS;     // коэффициент перевода ангстрем/dt в м/с
    inline constexpr float SpeedUnitToKmph = 1e-13f / kTimeUnitToHour;   // коэффициент перевода ангстрем/dt в км/ч
}
