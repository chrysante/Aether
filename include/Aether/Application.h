#ifndef AETHER_APPLICATION_H
#define AETHER_APPLICATION_H

#include <memory>

namespace xui {

class Application {};

__attribute__((weak)) std::unique_ptr<Application> createApplication();

} // namespace xui

#endif // AETHER_APPLICATION_H
