#include <memory>

#include <cstdio>

#include "Application.h"

int main(int argc, char *argv[]) {
  std::unique_ptr<Application> app;
  try {
    app = std::make_unique<Application>();
  } catch (std::exception &e) {
    fprintf(stderr, "Failed to initialize the application: %s\n", e.what());
    return -1;
  }
  return app->run(argc > 1 ? argv[1] : nullptr);
}
