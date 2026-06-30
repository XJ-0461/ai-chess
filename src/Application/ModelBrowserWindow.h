#pragma once

#include "Application/Model/ModelBrowserCache.hpp"
#include "Application/Model/ModelBrowserColumns.hpp"
#include "Application/Model/IModelBrowser.hpp"

namespace chess::application {

// Renders the cached models with a fetch trigger, price/throughput sorting,
// configurable columns, and a per-row right-click "copy model name" action.
void RenderModelBrowserWindow(
    bool* show,
    model::ModelBrowserCache& cache,
    model::IModelBrowser& open_router_browser,
    const model::ModelBrowserColumns& columns
);

// Renders the column-visibility checkboxes (intended for use inside a menu,
// e.g. View > Model Browser > Configure > Columns).
void RenderModelBrowserColumnsMenu(model::ModelBrowserColumns& columns);

} // namespace chess::application
