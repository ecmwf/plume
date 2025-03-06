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
#pragma once
#include <unordered_map>

#include "eckit/config/LocalConfiguration.h"
#include "eckit/config/YAMLConfiguration.h"
#include "eckit/filesystem/PathName.h"
#include "eckit/log/Log.h"

#include "atlas/field/FieldSet.h"
#include "atlas/functionspace/StructuredColumns.h"
#include "atlas/grid.h"
#include "atlas/grid/Distribution.h"

#include "base_data_reader.h"
#include "config_reader.h"
#include "grib_file_reader.h"

namespace nwp_emulator {

/**
 * @enum DataSourceType
 * @brief Enumeration of the supported types of data source for the emulator.
 */
enum DataSourceType
{
    GRIB,
    CONFIG,
    INVALID
};

/**
 * @class NWPDataProvider
 * @brief Requests model data from a data reader object, and provides it to the emulator as Atlas fields.
 *
 * Atlas is used to reproduce the partionioned environment of a NWP model run, as it has utilities to
 * scatter data across processes, and it is the data structure used to run Plume.
 * The parameters that the emulator will propose at each time step are locked at setup and live in params_
 * which stores information about the paramaters, level types and levels like so :
 * params_ = {"u" : {"sfc" : ["0"], "ml" : ["1","137"], "pl": ["850"]}}
 * In this example, the u component of the wind field has 4 levels in total in the emulator.
 */
class NWPDataProvider final {
private:
    const DataSourceType sourceType_;
    std::unique_ptr<BaseDataReader> dataReader = nullptr;
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<std::string>>> params_;

    std::string gridName_;
    atlas::functionspace::StructuredColumns fs_;
    atlas::FieldSet modelFieldSet_;

    size_t rank_;
    size_t root_;
    size_t nprocs_;

    /**
     * @brief Requests that each reader generates its own share of the model data.
     *
     * This method can be used only with compatible data sources. Having all processes reading and extracting
     * data from files like the GRIB reader is doing may be unstable or suboptimal. Conversely, the config reader
     * might be able to leverage other workers as the data is generated from a function.
     *
     * @return true if model data has been successfully generated for the step, false otherwise.
     */
    bool getLocalStepData();

    /**
     * @brief Requests to the root reader to generate and scatter the global model data.
     *
     * This method can be used with all data sources, but might be performing less efficiently than the data
     * source type may allow, because a single process is doing the actual data generation work, and there are
     * extra steps of gathering and scattering data across the partitioned fields.
     *
     * @return true if model data has been successfully generated for the step, false otherwise.
     */
    bool getGlobalStepData();

    /**
     * @brief Utility method to find where the raw values passed by the reader should be stored.
     *
     * It is assumed that the source data can have multiple level types and levels. Ideally, the surface level
     * should always have index 0, then next levels should be model levels (even though a 1:1 mapping cannot be
     * assumed because we may have ml 1,5,10 which will result in a 3-model levels emulator), then other levels
     * come in alphabetical order. In the above example for params_, the levels map to these indices :
     * sfc,0 -> 0 ; ml,1 -> 1 ; ml,137 -> 2 ; pl,850 -> 3
     *
     * @param field The field for which to find the level index.
     * @param levtype The name of the level type.
     * @param level The name of the level.
     *
     * @return int The index of the second shape of the Atlas field where the data should go.
     */
    int findLevelIndex(atlas::Field& field, std::string levtype, std::string level);

public:
    /**
     * @brief Constructs a NWPDataProvider object.
     *
     * Maintains an instance of a reader corresponding to the user provided setup. Instantiates all
     * the model Atlas function space, and fields.
     *
     * @param sourceType The type of data source to use for this emulator run.
     * @param inputPath The path to the config or source directory for the data reader.
     * @param rank The rank of the current process.
     * @param root The rank of the root process.
     * @param nprocs The total number of processors that run the emulator.
     *
     * @note The constructor aborts the execution unless its setup and the data reader setups are successful.
     */
    NWPDataProvider(const DataSourceType& sourceType, const eckit::PathName& inputPath, size_t rank, size_t root,
                    size_t nprocs);

    /**
     * @brief Populates Atlas fields with data for the next emulator step.
     *
     * @return bool True if there is still more emulator steps to run, false once all the data has been provided
     *              for the current emulator run.
     */
    bool getStepData();

    /// Getters
    atlas::FieldSet& getModelFieldSet() { return modelFieldSet_; }
    int getStep() const { return dataReader->getStep(); }
};
}  // namespace nwp_emulator