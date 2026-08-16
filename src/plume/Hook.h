/*
 * (C) Copyright 2023- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 *
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */
#pragma once


namespace plume {

/**
 * @brief The implicit hook point.
 *
 * It is always offered by every model, whether or not the model registers hook points of its own.
 * A plugin that requires no hook point is bound to it, and the hook-less Manager::run() targets
 * it. This is what makes hook-unaware models and plugins behave exactly as they did before hook
 * points existed.
 *
 * NOTE for models adopting hook points: keep invoking Manager::run() (or Manager::run(DEFAULT_HOOK))
 * at the point where the single run call used to be. A model that only invokes its own named hook
 * points leaves every plugin that declares no hook point silently dormant.
 */
constexpr const char* DEFAULT_HOOK = "default";

}  // namespace plume
