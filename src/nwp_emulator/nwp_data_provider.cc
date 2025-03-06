/*
 * (C) Copyright 2025- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 *
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */
#include <sstream>

#include "nwp_data_provider.h"

namespace nwp_emulator {
NWPDataProvider::NWPDataProvider(const DataSourceType& sourceType, const eckit::PathName& inputPath, size_t rank,
                                 size_t root, size_t nprocs) :
    sourceType_(sourceType), rank_(rank), root_(root), nprocs_(nprocs) {
    switch (sourceType_) {
        case DataSourceType::GRIB:
            eckit::Log::info() << "Emulator will use GRIB files as data source from " << inputPath << std::endl;
            dataReader = std::make_unique<GRIBFileReader>(inputPath, rank, root);
            break;
        case DataSourceType::CONFIG:
            eckit::Log::info() << "Emulator will use config as source type from" << inputPath << std::endl;
            dataReader = std::make_unique<ConfigReader>(inputPath, rank, root);
            break;
        default:
            eckit::Log::error() << "Emulator source type '" << sourceType_ << "' not supported, exit..." << std::endl;
            eckit::mpi::comm().abort(1);
    }

    // Some logging for sanity
    eckit::Log::info() << "Process " << rank << " has grid name " << dataReader->getGridName() << std::endl;
    eckit::Log::info() << "Process " << rank << " has params " << dataReader->getParams() << std::endl;

    gridName_ = dataReader->getGridName();
    // Parse parameters into a dictionary structure
    for (const auto& paramMd : dataReader->getParams()) {
        std::stringstream ss(paramMd);
        std::string shortName, levtype, level;

        // Read each part separated by comma
        std::getline(ss, shortName, ',');
        std::getline(ss, levtype, ',');
        std::getline(ss, level, ',');

        params_[shortName][levtype].push_back(level);
    }

    // Find the maximum number of levels across all params
    size_t nlevels = 1;
    for (const auto& levtypePair : params_) {
        size_t paramlevels = 0;
        for (const auto& levelPair : levtypePair.second) {
            paramlevels += levelPair.second.size();
        }
        nlevels = std::max(nlevels, paramlevels);
    }
    std::map<std::string, int> fieldsMd;
    for (const auto& levtypePair : params_) {
        fieldsMd[levtypePair.first] = 1;  // 2D fields receive 1 level
        if (levtypePair.second.size() > 1 || levtypePair.second.begin()->second.size() > 1) {
            // All 3D fields receive the same number of levels even if some levels remain empty for some fields
            fieldsMd[levtypePair.first] = nlevels;
        }
    }

    // Atlas grid & function space
    eckit::mpi::comm().barrier();  // Make sure all processes start creating their function space together
    atlas::Grid grid(gridName_);
    // Use the default partitioner for the given grid type
    atlas::grid::Distribution distribution(grid, atlas::util::Config("type", grid.partitioner().getString("type")) |
                                                     atlas::util::Config("bands", nprocs_));
    fs_ = atlas::functionspace::StructuredColumns(grid, distribution, atlas::util::Config("levels", nlevels));
    if (sourceType_ == DataSourceType::CONFIG) {
        dataReader->setReaderArea(fs_.lonlat());
    }

    // Create model fields
    for (const auto& fieldMd : fieldsMd) {
        auto field = modelFieldSet_.add(fs_.createField<FIELD_TYPE_REAL>(atlas::option::name(fieldMd.first) |
                                                                         atlas::option::levels(fieldMd.second)));
        std::vector<std::string> levelOrder;
        for (const auto& levelPair : params_[field.name()]) {
            levelOrder.push_back(levelPair.first);  // preferred order should be sfc, ml then other types
        }
        std::sort(levelOrder.begin(), levelOrder.end(), [](const std::string& a, const std::string& b) {
            // Priority for "sfc" and "ml", sort other levtypes alphabetically
            if (a == "sfc") {
                return true;
            }
            if (a == "ml") {
                if (b == "sfc") {
                    return false;
                }
                return true;
            }
            return a < b;
        });
        // This piece of metadata is used by the provider to populate the right levels, it should not be used by plugins
        field.metadata().set("levelOrder", levelOrder);
        std::cout << "Process " << rank_ << " created field '" << fieldMd.first << "' with shape " << field.shape()
                  << std::endl;
    }
}

bool NWPDataProvider::getStepData() {
    if (dataReader->done()) {
        return false;
    }
    switch (sourceType_) {
        case DataSourceType::GRIB:
            return getGlobalStepData();
        case DataSourceType::CONFIG:
            return getLocalStepData();
    }
    return false;
}

bool NWPDataProvider::getGlobalStepData() {
    // Create a temporary global field set to populate all the data and gather model fields into it
    atlas::FieldSet tmpGlobalFieldSet;
    for (size_t i = 0; i < modelFieldSet_.size(); ++i) {
        auto tmpField = tmpGlobalFieldSet.add(fs_.createField(
            atlas::option::name(modelFieldSet_[i].name()) | atlas::option::datatype(modelFieldSet_[i].datatype()) |
            atlas::option::levels(modelFieldSet_[i].levels()) |
            atlas::option::variables(modelFieldSet_[i].variables()) | atlas::option::global(root_)));
        tmpField.metadata().set("levelOrder", modelFieldSet_[i].metadata().getStringVector("levelOrder"));
    }
    fs_.gather(modelFieldSet_, tmpGlobalFieldSet);

    // Populate the tmp global field set with data from the reader
    std::string shortName, levtype, level;
    std::vector<FIELD_TYPE_REAL> values;
    while (dataReader->nextMessage(shortName, levtype, level, values)) {
        // We don't verify what params are passed because the setup step in the reader allows the execution only
        // if all source files have exactly the same set of fields on the same grid
        if (rank_ == root_) {
            auto source   = atlas::array::make_view<FIELD_TYPE_REAL, 2>(tmpGlobalFieldSet.field(shortName));
            int level_idx = findLevelIndex(tmpGlobalFieldSet.field(shortName), levtype, level);
            for (atlas::idx_t i_pt = 0; i_pt < tmpGlobalFieldSet.field(shortName).shape(0); i_pt++) {
                source(i_pt, level_idx) = values[i_pt];
            }
        }
    }
    fs_.scatter(tmpGlobalFieldSet, modelFieldSet_);
    return true;
}

bool NWPDataProvider::getLocalStepData() {
    std::string shortName, levtype, level;
    std::vector<FIELD_TYPE_REAL> values;
    while (dataReader->nextMessage(shortName, levtype, level, values)) {
        auto source   = atlas::array::make_view<FIELD_TYPE_REAL, 2>(modelFieldSet_.field(shortName));
        int level_idx = findLevelIndex(modelFieldSet_.field(shortName), levtype, level);
        for (atlas::idx_t i_pt = 0; i_pt < modelFieldSet_.field(shortName).shape(0); i_pt++) {
            source(i_pt, level_idx) = values[i_pt];
        }
    }
    return true;
}

int NWPDataProvider::findLevelIndex(atlas::Field& field, std::string levtype, std::string level) {
    int level_idx =
        std::distance(params_[field.name()][levtype].begin(),
                      std::find(params_[field.name()][levtype].begin(), params_[field.name()][levtype].end(), level));
    // level_idx + size of previous levtypes in levelOrder
    std::vector<std::string> levelOrder = field.metadata().getStringVector("levelOrder");
    for (int type_idx = 0; type_idx < levelOrder.size(); type_idx++) {
        if (levelOrder[type_idx] == levtype) {
            break;
        }
        level_idx += params_[field.name()][levelOrder[type_idx]].size();
    }
    return level_idx;
}
}  // namespace nwp_emulator