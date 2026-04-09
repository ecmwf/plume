# Plume Data

## Adding a New Concrete Update Strategy

This section describes the steps required to add a new concrete implementation of an update strategy.
Please follow **all steps** to ensure the class is properly registered, configurable, and tested.

---

### Checklist

When adding a new concrete update strategy class, you will need to:

1. Create the new class inheriting from the base class `UpdateStrategy` in [FieldProvider.h](./FieldProvider.h)
   > \[!TIP\]
   > At the end of the `update` implementation, make sure to trigger an updated flag switch to true for the downstream
   plugins relying on that information.
2. Define its type trait by specialising `UpdateStrategyTraits` primary template
   > \[!IMPORTANT\]
   > Ensure the strategy name is unique
3. If needed, update `StrategyArgs` to make sure all the constructor argument types are in the variant
4. Extend the allowed configuration options in the `allConfigArgs` array of the primary template `UpdateStrategyTraits`
5. To ensure the strategy can be negotiated by plugins, add its trait to `AllUpdateStrategyTraits` tuple
6. Register the class in the `ModelData` constructor
7. Add unit tests in [test_update_strategies.cc](../../../tests/core/test_update_strategies.cc)
