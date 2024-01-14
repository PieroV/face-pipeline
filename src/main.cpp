/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

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
