#include "CreateDebugPanels.h"

#include "App/debug/ProfilerTreeView.h"
#include "GUI/interface/panels/debug/DebugDrawers.h"
#include "GUI/interface/panels/debug/DebugPanel.h"
#include "GUI/interface/panels/debug/view/DebugView.h"

namespace {
    static DebugView* buildDebugSimView(DebugPanel& panel) {
        return panel.addView(DebugView("Симуляция", {
            DebugValue("Память (МБ)", DebugDrawers::Float<2>),
            DebugValue("Рендер (мс)", DebugDrawers::Float<4>),
            DebugValue("Физика (мс)", DebugDrawers::Float<4>),
            DebugValue("Тип интегратора", DebugDrawers::String),
            DebugValue("Шаги симуляции", DebugDrawers::Int),
            DebugValue("Шагов/с", DebugDrawers::Float<2>),
            DebugValue("Время симуляции (ns)", DebugDrawers::Float<4>),
            DebugValue("Количество атомов", DebugDrawers::Int),
            DebugValue("Полная энергия (pj)", DebugDrawers::Float<4>),
            DebugValue("Температура (K)", DebugDrawers::Float<4>),
            DebugValue("Температура (°C)", DebugDrawers::Float<4>),
            DebugSeries("Полная средняя энергия (eV)"),
            DebugValue("Средняя скорость (км/ч)", DebugDrawers::Float<3>)
        }));
    }

    static DebugView* buildDebugAtomSingle(DebugPanel& panel) {
        return panel.addView(DebugView("Атом", {
            DebugValue("Позиция", DebugDrawers::Vec3<3>),
            DebugValue("Скорость (A/dt)", DebugDrawers::Float<3>),
            DebugValue("Скорость (м/с)", DebugDrawers::Float<3>),
            DebugValue("Скорость (км/ч)", DebugDrawers::Float<3>),
            DebugValue("Силы", DebugDrawers::Vec3<3>),
            DebugValue("Пред. силы", DebugDrawers::Vec3<3>),
            DebugValue("Потенциальная энергия", DebugDrawers::Float<4>),
            DebugValue("Масса", DebugDrawers::Float<3>),
            DebugValue("Радиус", DebugDrawers::Float<3>),
            DebugValue("Тип", DebugDrawers::Int),
        }));
    }

    static DebugView* buildDebugAtomBatch(DebugPanel& panel) {
        return panel.addView(DebugView("Атомы", {
            DebugValue("Выбрано атомов", DebugDrawers::Int),
        }));
    }

    static DebugView* buildDebugNeighborView(DebugPanel& panel) {
        return panel.addView(DebugView("NL", {
            DebugValue("Размер сетки", DebugDrawers::String),
            DebugValue("Размер бокса (nm)", DebugDrawers::String),
            DebugValue("Размер ячейки", DebugDrawers::Int),
            DebugValue("NeighborList включен", DebugDrawers::String),
            DebugValue("Память AtomStorage (МБ)", DebugDrawers::Float<3>),
            DebugValue("Память NeighborList (МБ)", DebugDrawers::Float<3>),
            DebugValue("Память SpatialGrid (МБ)", DebugDrawers::Float<3>),
            // Эти четыре — собственная статистика CPU NeighborList. В GPU-режиме CPU
            // NL не перестраивается (NL живёт в VRAM), а GPU-аналога без дорогого
            // per-frame readback нет, поэтому в GPU-режиме показываем честную метку
            // «н/д», а не застывшее CPU-число. String-дровер: значение форматирует
            // updateSimulationDebug (число в CPU-режиме / метка в GPU-режиме).
            DebugValue("Пар в NL", DebugDrawers::String),
            DebugValue("Ср. соседей на атом", DebugDrawers::String),
            DebugValue("Cutoff", DebugDrawers::Float<3>),
            DebugValue("Skin", DebugDrawers::Float<3>),
            DebugValue("List radius", DebugDrawers::Float<3>),
            DebugValue("Ребилдов NL", DebugDrawers::Int),
            DebugValue("Шагов между ребилдами (recent)", DebugDrawers::String),
            DebugValue("Время ребилда NL (мс)", DebugDrawers::String),
            DebugValue("SG заполненных ячеек", DebugDrawers::Int),
            DebugValue("SG макс атомов в ячейке", DebugDrawers::Int),
            DebugValue("SG ср. атомов/ячейку", DebugDrawers::Float<3>),
        }));
    }

    static DebugView* buildDebugProfilerView(DebugPanel& panel) { return panel.addView(DebugView("Profiler", drawProfilerTreeView)); }
}

DebugViews createDebugViews(DebugPanel& panel) {
    return DebugViews{
        buildDebugSimView(panel),      buildDebugAtomSingle(panel),   buildDebugAtomBatch(panel),
        buildDebugNeighborView(panel), buildDebugProfilerView(panel),
    };
}
