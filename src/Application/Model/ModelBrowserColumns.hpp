#pragma once

namespace chess::application::model {

// Which columns the Model Browser table shows. Toggled from the main menu
// (View > Model Browser > Configure > Columns) and consumed by the window.
// Performance columns default off since the /models endpoint does not populate
// them yet.
struct ModelBrowserColumns {
    bool provider{true};
    bool model{true};
    bool input_price{true};
    bool output_price{true};
    bool context_length{true};
    bool throughput{true};
    bool latency{false};
    bool time_to_first_token{false};
};

} // namespace chess::application::model
