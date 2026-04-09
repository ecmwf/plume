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
#include "plume/data/ParameterValue.h"
#include "eckit/config/LocalConfiguration.h"

namespace plume {
namespace data {

const std::string IParameterObserver::SEP_ = ";";

void IParameterObserver::setSubject(std::shared_ptr<IParameterObservable> subject) {
    auto currentSubject = subject_.lock();
    if (currentSubject == subject) {
        return;
    }

    if (currentSubject) {  // Detach from old subject if any
        currentSubject->detach(shared_from_this());
    }
    subject_ = subject;
    if (subject) {  // Attach to new subject if not null
        subject->attach(shared_from_this());
    }
}

std::string IParameterObserver::deriveParamName(const std::string& source, const std::string& levtype,
                                                const std::string& level) {
    if (levtype != "hl" && levtype != "dummy") {  // dummy is allowed for testing
        throw eckit::BadValue("Plume derived params only supports levtype 'hl'!", Here());
    }
    return source + SEP_ + levtype + SEP_ + level;  // default is 'name;levtype;level'
}

void IParameterObservable::notifyObservers() {
    // clean up expired observers first & then notify alive observers
    observers_.erase(std::remove_if(observers_.begin(), observers_.end(),
                                    [](const std::weak_ptr<IParameterObserver>& w) { return w.expired(); }),
                     observers_.end());

    for (auto& w : observers_) {
        if (auto obs = w.lock()) {
            obs->onSubjectChanged();
        }
    }
}

void IParameterObservable::attach(std::shared_ptr<IParameterObserver> observer) {
    // avoid duplicates & nullptrs
    if (!observer)
        return;
    auto it = std::find_if(observers_.begin(), observers_.end(),
                           [&](const std::weak_ptr<IParameterObserver>& w) { return w.lock() == observer; });

    if (it == observers_.end()) {
        observers_.push_back(observer);
    }
}

void IParameterObservable::detach(std::shared_ptr<IParameterObserver> observer) {
    observers_.erase(std::remove_if(observers_.begin(), observers_.end(),
                                    [&](const std::weak_ptr<IParameterObserver>& w) { return w.lock() == observer; }),
                     observers_.end());
}


}  // namespace data
}  // namespace plume