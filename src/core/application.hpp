// Copyright (c) Luster
#pragma once

#include "core/core.hpp"
#include "core/window.hpp"

namespace luster {

class Renderer;

class Application {
public:
  Application();
  ~Application();

  void run();

private:
  void init();
  void mainLoop();
  void cleanup();

  Renderer *renderer_ = nullptr;
  Window *window_ = nullptr;
};

} // namespace luster
