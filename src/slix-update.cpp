#include "App.h"
#include "slix.h"
#include "utils.h"

#include <clice/clice.h>
#include <stdexcept>
#include <fstream>
#include <iostream>

namespace {
void app();
auto cli = clice::Argument{ .args   = "update",
                            .desc   = "updates the known packages from mirrors/remotes",
                            .cb     = app,
};
void app() {
    auto app = App{
        .verbose = cliVerbose,
    };
    app.init();

    // just update the remote package, not much to do here
    app.update();
}
}
