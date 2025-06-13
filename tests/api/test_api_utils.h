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

#include "eckit/testing/Test.h"
#include "plume/api/plume.h"


using namespace eckit::testing;

namespace plume::test {

#define EXPECT_PLUME_CODE_SUCCESS(error_expr)                      \
    do {                                                           \
        int error_code = error_expr;                               \
        EXPECT_EQUAL(error_code, PlumeErrorValues::PLUME_SUCCESS); \
    } while (false)


#define EXPECT_PLUME_CODE_FAILURE(error_expr)                          \
    do {                                                               \
        int error_code = error_expr;                                   \
        EXPECT_NOT_EQUAL(error_code, PlumeErrorValues::PLUME_SUCCESS); \
    } while (false)

} // namespace plume::test

