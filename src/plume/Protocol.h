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

#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "eckit/config/Configuration.h"
#include "plume/Hook.h"
#include "plume/data/ParameterCatalogue.h"
#include "plume/data/ParameterType.h"


namespace plume {

/**
 * @brief Used to match plugin requests and model offers
 */
class Protocol {
private:
    // internal utility functions
    void insertParam(const data::ParameterDefinition& param, data::ParameterCatalogue& catalogue);

    // requireAt()/requireWritableAt()/offerAt() take an explicit scope: reject an empty one rather than silently
    // treating it as unscoped
    static void checkHookScope(const std::string& name, const std::set<std::string>& hooks);

    void setParamsFromConfig(const eckit::Configuration& config);

    void setHooksFromConfig(const eckit::Configuration& config);

    std::string requestedPlumeVersion_;
    std::string requestedAtlasVersion_;
    data::ParameterCatalogue requiredParams_;
    std::set<std::string> requiredHooks_;

    std::string offeredPlumeVersion_;
    std::string offeredAtlasVersion_;
    data::ParameterCatalogue offeredParams_;

    // offered hook points: name -> comment
    std::map<std::string, std::string> offeredHooks_;

public:
    /**
     * @brief Construct a new Protocol object
     *
     */
    Protocol();

    /**
     * @brief Construct a new Protocol object
     *
     * @param config
     */
    Protocol(const eckit::Configuration& config);

    /**
     * @brief Destroy the Protocol object
     *
     */
    ~Protocol();


    // ------- required
    void requirePlumeVersion(const std::string& version);
    void requireAtlasVersion(const std::string& version);

    template <typename T>
    void require(const std::string& name) {
        insertParam(data::ParameterDefinition(name, data::deduceType<T>()), requiredParams_);
    }

    /**
     * @brief Lets plugins require write access to a parameter.
     *
     * The negotiator will reject the plugin if the model has not offered the parameter as writable.
     */
    template <typename T>
    void requireWritable(const std::string& name) {
        insertParam(data::ParameterDefinition(name, data::deduceType<T>(), /*avail=*/"", /*comment=*/"",
                                              /*writable=*/true),
                    requiredParams_);
    }

    /**
     * @brief Lets plugins require params derived from model parameters in their negotiate method.
     */
    template <typename T>
    void require(const std::string& name, const std::unordered_map<std::string, std::string>& options) {
        insertParam(data::ParameterDefinition(name, data::deduceType<T>(), options), requiredParams_);
    }

    /**
     * @brief Lets plugins require a param only at some of the hook points they are bound to.
     *
     * A param required through require() is needed at every hook point the plugin is bound to. With requireAt()
     * the model is only told to keep it current at the listed hook points, and negotiation checks that the model
     * offers it there. Every listed hook point must be one the plugin is bound to, otherwise the plugin is rejected.
     * Requiring the same param several times unites the hook scopes (an unscoped requirement wins).
     *
     * @param hooks non-empty set of hook point names
     */
    template <typename T>
    void requireAt(const std::string& name, const std::set<std::string>& hooks) {
        checkHookScope(name, hooks);
        insertParam(data::ParameterDefinition(name, data::deduceType<T>(), "", "", /*writable=*/false, hooks),
                    requiredParams_);
    }

    /**
     * @brief Lets plugins require write access to a param only at some of the hook points they are bound to.
     *
     * The param is writable at the listed hook points. Combined with requireAt() on the same param, a plugin can
     * read a param at some hook points and write it at others, e.g.:
     * @code
     * protocol.requireWritableAt<atlas::Field>("P", {"H1"});  // written at H1
     * protocol.requireAt<atlas::Field>("P", {"H2"});          // read-only at H2
     * @endcode
     * Repeated definitions of the same param are merged per hook point: writable wins (see
     * data::ParameterDefinition::mergeAccess). Derived params cannot be writable, hence there is no overload taking
     * strategy options.
     *
     * @param hooks non-empty set of hook point names
     */
    template <typename T>
    void requireWritableAt(const std::string& name, const std::set<std::string>& hooks) {
        checkHookScope(name, hooks);
        insertParam(data::ParameterDefinition(name, data::deduceType<T>(), "", "", /*writable=*/true, hooks),
                    requiredParams_);
    }

    /**
     * @brief As requireAt() above, for a param derived from model parameters (see require() with options).
     */
    template <typename T>
    void requireAt(const std::string& name, const std::unordered_map<std::string, std::string>& options,
                   const std::set<std::string>& hooks) {
        checkHookScope(name, hooks);
        insertParam(data::ParameterDefinition(name, data::deduceType<T>(), options, hooks), requiredParams_);
    }

    /**
     * @brief Lets plugins require to be executed at a specific hook point of the model.
     *
     * A plugin that requires no hook point is bound to plume::DEFAULT_HOOK. Hook requirements are
     * all-or-nothing: if the model does not offer one of the required hook points, the plugin is
     * rejected.
     */
    void requireHook(const std::string& name);

    std::set<std::string> requiredParamNames() const;
    const std::string& requiredPlumeVersion() const;
    const std::string& requiredAtlasVersion() const;
    const std::set<std::string>& requiredHooks() const;

    bool isParamRequired(const std::string& name) const;
    bool isHookRequired(const std::string& name) const;
    const data::ParameterCatalogue& requires() const;


    // ------- offered
    void offerPlumeVersion(const std::string& version);
    void offerAtlasVersion(const std::string& version);

    /**
     * @brief Lets the model offer a param at every hook point it registers.
     *
     * @param writable whether plugins may write the param back (subject to the write-back policy)
     */
    template <typename T>
    void offer(const std::string& name, const std::string& avail, const std::string& comment, bool writable = false) {
        insertParam(data::ParameterDefinition(name, data::deduceType<T>(), avail, comment, writable), offeredParams_);
    }

    /**
     * @brief Lets the model offer a param only at some hook points.
     *
     * Offering the same param several times merges the definitions per hook point (writable wins), so a param can
     * be offered writable at some hook points and read-only at others:
     * @code
     * offers.offerAt<atlas::Field>("P", "always", "", {"H1"}, true);  // writable at H1
     * offers.offerAt<atlas::Field>("P", "always", "", {"H2"});        // read-only at H2
     * @endcode
     *
     * @param hooks    non-empty hook scope: the hook points at which the model keeps this param current. Each listed
     *                 hook point must be registered with offerHook() by the time Manager::negotiate() is called.
     * @param writable whether plugins may write the param back at these hook points
     */
    template <typename T>
    void offerAt(const std::string& name, const std::string& avail, const std::string& comment,
                 const std::set<std::string>& hooks, bool writable = false) {
        checkHookScope(name, hooks);
        insertParam(data::ParameterDefinition(name, data::deduceType<T>(), avail, comment, writable, hooks),
                    offeredParams_);
    }

    /**
     * @brief Lets the model register a hook point that it is able to call Plume from.
     *
     * plume::DEFAULT_HOOK is always registered, so a model that never calls this still offers it.
     * Registering an already registered hook point - including the default one - only updates its
     * comment.
     */
    void offerHook(const std::string& name, const std::string& comment = "");

    std::set<std::string> offeredParamNames() const;
    const std::string& offeredPlumeVersion() const;
    const std::string& offeredAtlasVersion() const;
    std::set<std::string> offeredHookNames() const;
    const std::map<std::string, std::string>& offeredHooks() const;

    bool isParamOffered(const std::string& name) const;
    bool isHookOffered(const std::string& name) const;
    const data::ParameterCatalogue& offers() const;

    /**
     * @brief Hook points at which an offered param is available: its hook scope, or every offered hook point if
     *        it is unscoped. Empty if the param is not offered.
     */
    std::set<std::string> offeredHooksFor(const std::string& name) const;
};


}  // namespace plume