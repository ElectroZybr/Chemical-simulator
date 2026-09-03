#pragma once

#include "SoA.hpp"
#include <Lattice/Kernel/Components.hpp>
#include <Lattice/Kernel/Exception.hpp>

#include "StdIo/include/Document.hpp"
#include "StdIo/include/LoaderAPI.hpp"


class SoALoader final : public LoaderAPI {
public:
    void configure(Lattice::Components& branch) {
        branch_ = &branch;
    }

    void load(const Document& doc) override {
        const auto* data = doc.get("SoAData");

        if (!data || !data->is<Table>())
            return;

        const auto& table = data->as<Table>();

        const auto targetIt = table.find("target");
        const auto columnsIt = table.find("columns");
        const auto rowsIt = table.find("rows");

        if (targetIt == table.end() || !targetIt->second.is<std::string>())
            throw Lattice::Exception("SoALoader", "SoAData.target is missing");

        if (columnsIt == table.end() || !columnsIt->second.is<Array>())
            throw Lattice::Exception("SoALoader", "SoAData.columns is missing");

        if (rowsIt == table.end() || !rowsIt->second.is<Array>())
            throw Lattice::Exception("SoALoader", "SoAData.rows is missing");

        const auto& target = targetIt->second.as<std::string>();
        const auto& columnNames = columnsIt->second.as<Array>();
        const auto& rowData = rowsIt->second.as<Array>();

        Ref<StdData::SoA> soa = branch_->require<StdData::SoA>(target);

        if (columnNames.empty())
            return;

        // Проверяем, что все колонки существуют в SoA.
        for (const auto& value : columnNames) {
            const auto* name = std::get_if<std::string>(&value);

            if (!name)
                throw Lattice::Exception("SoALoader", "Column name must be a string");

            if (!soa->get(*name))
                throw Lattice::Exception("SoALoader", "Column '{}' not found in SoA", *name);
        }

        // Добавляем строки.
        soa->resize(soa->size() + rowData.size());

        const size_t offset = soa->size() - rowData.size();

        for (size_t row = 0; row < rowData.size(); ++row) {
            const auto* values = std::get_if<Array>(&rowData[row]);

            if (!values)
                throw Lattice::Exception("SoALoader", "Each row must be an array");

            if (values->size() != columnNames.size())
                throw Lattice::Exception("SoALoader", "Row {} has {} values, expected {}", row, values->size(), columnNames.size());

            for (size_t column = 0; column < columnNames.size(); ++column) {
                const auto& nameValue = columnNames[column];

                if (!std::holds_alternative<std::string>(nameValue))
                    throw Lattice::Exception("SoALoader", "Column name must be a string");

                const auto& name = std::get<std::string>(nameValue);

                soa->set(name, offset + row, (*values)[column]);
            }
        }
    }
private:
    Ref<Lattice::Components> branch_;
};