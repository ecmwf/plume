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
#include <math.h>
#include <algorithm>
#include <random>

#include "atlas/array/ArrayView.h"
#include "atlas/util/CoordinateEnums.h"
#include "atlas/util/function/VortexRollup.h"

#include "config_reader.h"
#include "nwp_definitions.h"

/**
 * @brief Computes the cardinal sine of a 2D lon,lat point.
 *
 * @param x The point repreented by its longitude and latitude.
 * @param centre The point where the peak of the function is located.
 * @param spread The spread of the function blob (the bigger spread the tighter the blob).
 *
 * @return The sinc value at the given point as double or float.
 */
FIELD_TYPE_REAL sinc2d(atlas::PointLonLat x, atlas::PointLonLat centre, double spread) {
    double sphericalDist = std::acos(std::sin(x.lat() * M_PI / 180.0) * std::sin(centre.lat() * M_PI / 180.0) +
                                     std::cos(x.lat() * M_PI / 180.0) * std::cos(centre.lat() * M_PI / 180.0) *
                                         std::cos((x.lon() - centre.lon()) * M_PI / 180.0));
    if (sphericalDist <= 1e-6) {  // sinc(0) = 1
        return 1.0;
    }
    return std::sin(M_PI * sphericalDist * spread) / (M_PI * sphericalDist * spread);
}

/**
 * @brief Computes the gaussian of a 2D lon,lat point.
 *
 * @param x The point repreented by its longitude and latitude.
 * @param mu The point where the peak of the function is located.
 * @param sigma The spread of the function blob.
 *
 * @return The gaussian value at the given point as double or float.
 */
FIELD_TYPE_REAL gaussian2d(atlas::PointLonLat x, atlas::PointLonLat mu, std::pair<double, double> sigma) {
    return std::exp(-(0.5 * std::pow((x.lat() - mu.lat()) / sigma.second, 2) +
                      0.5 * std::pow((x.lon() - mu.lon()) / sigma.first, 2)));
}

namespace nwp_emulator {
//----------------------------------------------------------------------------------------------------------------------
// Data generation functions & utils
//----------------------------------------------------------------------------------------------------------------------
void ConfigReader::sampleNCentres(const eckit::LocalConfiguration& options, std::vector<atlas::PointLonLat>& centres) {
    auto nmodes = options.getInt("modes");
    std::vector<double> tmp(nmodes * 2);  // Use a temporary centres storage as PointLonLat can't be broadcast
    if (rank_ == root_) {                 // Only the root does the random sampling so it's consistent across partitions
        std::random_device rd;
        std::mt19937 generator(rd());
        double minLat = -90.0, maxLat = 90.0, minLon = 0.0, maxLon = 360.0;
        std::vector<double> areaCoords;
        if (options.has("area")) {
            areaCoords = options.getDoubleVector("area");
        }
        else if (readerConfig_.has("area")) {
            areaCoords = readerConfig_.getDoubleVector("area");
        }
        if (!areaCoords.empty()) {
            // Assumes the user defined area does not spread over the wraping 0/360 longitude
            // Would need two lon ranges to support spreading over lon 0
            minLon = areaCoords[1] + 180.0;  // west
            maxLon = areaCoords[3] + 180.0;  // east
            minLat = areaCoords[2];          // south
            maxLat = areaCoords[0];          // north
        }
        std::uniform_real_distribution<double> distLat(minLat, maxLat);
        std::uniform_real_distribution<double> distLon(minLon, maxLon);
        for (int i = 0; i < nmodes; i++) {
            tmp[2 * i]     = distLon(generator);
            tmp[2 * i + 1] = distLat(generator);
        }
    }
    eckit::mpi::comm().broadcast(tmp, root_);  // Broadcast the sampled centres
    for (int i = 0; i < nmodes; i++) {
        centres.push_back(atlas::PointLonLat{tmp[2 * i], tmp[2 * i + 1]});
    }
}

std::unique_ptr<FocusArea> ConfigReader::setFocusArea(const eckit::LocalConfiguration& options) {
    std::vector<double> translation{0.0, 0.0};
    if (options.has("translation")) {
        translation = options.getDoubleVector("translation");
        translation[0] *= step_;
        translation[1] *= step_;
    }
    std::vector<double> areaCoords;
    std::unique_ptr<FocusArea> focusArea = nullptr;
    if (options.has("area")) {
        areaCoords = options.getDoubleVector("area");
    }
    else if (readerConfig_.has("area")) {
        areaCoords = readerConfig_.getDoubleVector("area");
    }
    if (!areaCoords.empty()) {
        // clamp latitudes to avoid misinterpreting the area when it touches a pole
        double north = std::max(-90.0, std::min(90.0, areaCoords[0] + translation[0]));
        double south = std::max(-90.0, std::min(90.0, areaCoords[2] + translation[0]));
        // normalise longitudes between [0, 360]
        double west = std::fmod(areaCoords[1] + translation[1] + 180.0, 360.0);
        double east = std::fmod(areaCoords[3] + translation[1] + 180.0, 360.0);
        if (west < 1e-6) {
            west += 360.0;
        }
        if (east < 1e-6) {
            east += 360.0;
        }
        focusArea = std::make_unique<FocusArea>(west, east, south, north);
    }
    return focusArea;
}

void ConfigReader::applyVortexRollup(const eckit::LocalConfiguration& options, std::vector<FIELD_TYPE_REAL>& values) {
    // options are time_variation + area (optional)
    auto timeVariation                   = options.getDouble("time_variation") * step_;
    auto lonlat                          = atlas::array::make_view<double, 2>(readerArea_);
    std::unique_ptr<FocusArea> focusArea = setFocusArea(options);
    for (size_t i_pt = 0; i_pt < values.size(); i_pt++) {
        atlas::PointLonLat p{lonlat(i_pt, atlas::LON), lonlat(i_pt, atlas::LAT)};
        if (focusArea && !focusArea->contains(p)) {
            continue;
        }
        // populate only the values if (lon,lat) belongs to the focus area or there is no mask
        values[i_pt] = atlas::util::function::vortex_rollup(p.lon(), p.lat(), timeVariation);
    }
}

void ConfigReader::applyRandom(const eckit::LocalConfiguration& options, std::vector<FIELD_TYPE_REAL>& values) {
    // options are all optional: area, distribution, distribution specific options
    auto lonlat                          = atlas::array::make_view<double, 2>(readerArea_);
    std::unique_ptr<FocusArea> focusArea = options.getBool("use_area", true) ? setFocusArea(options) : nullptr;

    std::random_device rd;
    std::mt19937 generator(rd());
    auto distribution_type = options.getString("distribution", "uniform");
    std::vector<FIELD_TYPE_REAL> randomValues(values.size());
    // 1. Generate random values for the whole field covered by the partition
    if (distribution_type == "bernoulli") {  // This one is used for random step
        auto probability             = options.getDouble("probability", 0.5);
        FIELD_TYPE_REAL successValue = options.getDouble("value", 1.0);
        std::bernoulli_distribution distribution(probability);
        std::generate(randomValues.begin(), randomValues.end(),
                      [&]() { return distribution(generator) ? successValue : 0.0; });
    }
    else if (distribution_type == "uniform") {
        auto min = options.getDouble("min", 0.0);
        auto max = options.getDouble("max", 1.0);
        std::uniform_real_distribution<double> distribution(min, max);
        std::generate(randomValues.begin(), randomValues.end(),
                      [&]() { return static_cast<FIELD_TYPE_REAL>(distribution(generator)); });
    }
    else if (distribution_type == "normal") {
        auto mean   = options.getDouble("mean", 0.5);
        auto stddev = options.getDouble("stddev", 1.0);
        std::normal_distribution<double> distribution(mean, stddev);
        std::generate(randomValues.begin(), randomValues.end(),
                      [&]() { return static_cast<FIELD_TYPE_REAL>(distribution(generator)); });
    }

    // Apply randomly generated values only on the desired area
    for (size_t i_pt = 0; i_pt < values.size(); i_pt++) {
        atlas::PointLonLat p{lonlat(i_pt, atlas::LON), lonlat(i_pt, atlas::LAT)};
        if (focusArea && !focusArea->contains(p)) {
            continue;
        }
        values[i_pt] = randomValues[i_pt];
    }
}

void ConfigReader::applyStep(const eckit::LocalConfiguration& options, std::vector<FIELD_TYPE_REAL>& values) {
    // options are value, area (optional), if area then translation (optional)
    // probability (optional), if set only a proportion of points in the area will receive the step
    FIELD_TYPE_REAL stepValue            = static_cast<FIELD_TYPE_REAL>(options.getDouble("value"));
    FIELD_TYPE_REAL variation            = static_cast<FIELD_TYPE_REAL>(options.getDouble("variation", 0.0));
    auto lonlat                          = atlas::array::make_view<double, 2>(readerArea_);
    std::unique_ptr<FocusArea> focusArea = setFocusArea(options);

    std::vector<FIELD_TYPE_REAL> randomValues(values.size());
    if (options.has("probability")) {
        // use a separate field of values to make sure no undesirable changes are applied to the final field
        eckit::LocalConfiguration randomOpts(options);
        randomOpts.set("distribution", "bernoulli");
        randomOpts.set("use_area", false);
        applyRandom(randomOpts, randomValues);
    }

    for (size_t i_pt = 0; i_pt < values.size(); i_pt++) {
        atlas::PointLonLat p{lonlat(i_pt, atlas::LON), lonlat(i_pt, atlas::LAT)};
        if (focusArea && !focusArea->contains(p)) {
            continue;
        }
        // Point should be assigned a value
        if (options.has("probability")) {
            values[i_pt] = std::abs(randomValues[i_pt]) > 1e-6 ? randomValues[i_pt] + variation * step_ : 0.0;
        }
        else {
            values[i_pt] = stepValue + variation * step_;
        }
    }
}

void ConfigReader::applyCardinalSine(const eckit::LocalConfiguration& options, std::vector<FIELD_TYPE_REAL>& values) {
    // options are modes, min value (defaults to 0), and max value (defaults to 1)
    // sink (true if the blobs should not be peaks), spread is how big the sinc blobs can be
    auto lonlat                          = atlas::array::make_view<double, 2>(readerArea_);
    std::unique_ptr<FocusArea> focusArea = setFocusArea(options);

    auto nmodes         = options.getInt("modes");
    auto spread         = options.getDouble("spread", 10.0);
    auto sink           = options.getBool("sink", false);
    FIELD_TYPE_REAL min = options.getDouble("min", 0.0);
    FIELD_TYPE_REAL max = options.getDouble("max", 1.0);

    // 1. Randomly determine N (nmodes) centres
    std::vector<atlas::PointLonLat> modesLonLat;
    sampleNCentres(options, modesLonLat);

    // 2. Randomly sample N spreads
    std::vector<double> spreads(nmodes);
    if (rank_ == root_) {
        std::random_device rd;
        std::mt19937 generator(rd());
        // The blob size is inverse proportional to the spread
        std::uniform_real_distribution<double> distribution(90.0 / spread, 180.0 / spread);
        for (size_t i = 0; i < nmodes; i++) {
            spreads[i] = distribution(generator);
        }
    }
    eckit::mpi::comm().broadcast(spreads, root_);

    // 3. Compute sinc
    for (size_t i_pt = 0; i_pt < values.size(); i_pt++) {
        atlas::PointLonLat p{lonlat(i_pt, atlas::LON), lonlat(i_pt, atlas::LAT)};
        if (focusArea && !focusArea->contains(p)) {
            continue;
        }
        FIELD_TYPE_REAL value = 0.0;
        for (size_t i = 0; i < nmodes; i++) {
            value += sinc2d(p, modesLonLat[i], spreads[i]);
        }
        if (sink) {  // scale and sign the result
            values[i_pt] = min + (nmodes - value) * (max - min) / (1.21723 * nmodes);
        }
        else {
            values[i_pt] = min + ((value + 0.21723 * nmodes) * (max - min)) / (1.21723 * nmodes);
        }
    }
}
void ConfigReader::applyGaussian(const eckit::LocalConfiguration& options, std::vector<FIELD_TYPE_REAL>& values) {
    // options are modes, max standard deviation (defaults to 1), sink (true if the blobs should not be peaks)
    // min value (defaults to 0), and max value (defaults to 1)
    auto lonlat                          = atlas::array::make_view<double, 2>(readerArea_);
    std::unique_ptr<FocusArea> focusArea = setFocusArea(options);

    auto nmodes         = options.getInt("modes");
    auto sink           = options.getBool("sink", false);
    FIELD_TYPE_REAL min = options.getDouble("min", 0.0);
    FIELD_TYPE_REAL max = options.getDouble("max", 1.0);
    auto max_stddev     = options.getDouble("max_stddev", 1.0);

    // 1. Randomly determine N (nmodes) centres
    std::vector<atlas::PointLonLat> modesLonLat;
    sampleNCentres(options, modesLonLat);

    // 2. Randomly sample N standard deviations
    std::vector<double> stddevs(2 * nmodes);
    if (rank_ == root_) {
        std::random_device rd;
        std::mt19937 generator(rd());
        std::uniform_real_distribution<double> distribution(1.0, max_stddev);
        for (size_t i = 0; i < nmodes; i++) {
            stddevs[2 * i]     = distribution(generator);
            stddevs[2 * i + 1] = distribution(generator);
        }
    }
    eckit::mpi::comm().broadcast(stddevs, root_);

    // 3. Compute Gaussian
    FIELD_TYPE_REAL value = 0.0;
    for (size_t i_pt = 0; i_pt < values.size(); i_pt++) {
        atlas::PointLonLat p{lonlat(i_pt, atlas::LON), lonlat(i_pt, atlas::LAT)};
        if (focusArea && !focusArea->contains(p)) {
            continue;
        }
        value = 0.0;
        for (size_t i = 0; i < nmodes; i++) {
            value += gaussian2d(p, modesLonLat[i], {stddevs[2 * i], stddevs[2 * i + 1]});
        }
        if (sink) {  // scale and sign the result
            values[i_pt] = min + (nmodes - value) * (max - min) / nmodes;
        }
        else {
            values[i_pt] = min + value * (max - min) / nmodes;
        }
    }
}
//----------------------------------------------------------------------------------------------------------------------
}  // namespace nwp_emulator