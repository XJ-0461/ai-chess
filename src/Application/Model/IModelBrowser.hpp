#pragma once

namespace chess::application::model {

// Abstraction over a provider-specific model catalogue. Implementations fetch
// their models asynchronously (via the shared TaskExecutor) and write the
// results into the shared ModelBrowserCache.
class IModelBrowser {
public:
    virtual ~IModelBrowser() = default;

    // Kicks off an asynchronous fetch; results land in the shared cache. Safe
    // to call from the UI thread - the work itself runs on the task pool.
    virtual void FetchModels() = 0;
};

} // namespace chess::application::model
