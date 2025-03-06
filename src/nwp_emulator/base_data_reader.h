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

#include <string>
#include <vector>

#include "atlas/field/Field.h"
#include "eckit/exception/Exceptions.h"
#include "nwp_definitions.h"

namespace nwp_emulator {

/**
 * @class BaseDataReader
 * @brief Base class for handling different types of data sources for the emulator.
 */
class BaseDataReader {
protected:
    std::string gridName_;
    std::vector<std::string> params_;

    int stepCountLimit_;  ///< Limitation on the number of steps the emulator can run
    int step_{0};         ///< Number of model steps
    int count_{0};        ///< Number of messages for the current step
    int index_{0};        ///< Index of the considered message at the current step

public:
    BaseDataReader(int stepCountLimit) : stepCountLimit_(stepCountLimit) {}
    virtual ~BaseDataReader() = default;

    /**
     * @brief Sets the reader lon,lat area if necessary for data generation.
     *
     * @param lonlat The lonlat field defininf the partition grid points.
     */
    virtual void setReaderArea(const atlas::Field& lonlat) {
        throw eckit::NotImplemented("This data reader does not support area definition", Here());
    }

    /**
     * @brief Provides the raw values for a single levelin the next message from the data source.
     *
     * Each of the inheriting readers will have a different way to generate raw model data for the provider,
     * which can use the parameters to map the raw values to a specific field, levtype and level.
     *
     * @param[out] shortName The name of the parameter represented by the raw values for the caller use.
     * @param[out] levtype The levtype of the parameter for the caller use.
     * @param[out] level The level of the levtype for the caller use.
     * @param[out] data The vector that contains the raw values for the field.
     *
     * @return true if the data generation is successful, false otherwise.
     */
    virtual bool nextMessage(std::string& shortName, std::string& levtype, std::string& level,
                             std::vector<FIELD_TYPE_REAL>& data) = 0;

    /// Returns true when the reader has read model data for all the steps.
    virtual bool done() = 0;

    /// Getters
    const std::string& getGridName() const { return gridName_; }
    int getStep() const { return step_; }
    const std::vector<std::string>& getParams() const { return params_; }
};
}  // namespace nwp_emulator