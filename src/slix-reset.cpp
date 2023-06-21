#include "App.h"
#include "slix.h"

#include <clice/clice.h>

namespace {
void app();
auto cli = clice::Argument{ .args   = "reset",
                            .desc   = "initializes slix for the local user, by setting up a default upstream",
                            .cb     = app,
};

void app() {
    auto app = App {
        .verbose = cliVerbose,
    };
    app.init(/*.reset=*/true);
}

}
