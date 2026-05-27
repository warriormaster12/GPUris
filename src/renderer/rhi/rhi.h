#include <cstdint>
class RHI {
public:
  virtual void prepareFrame() = 0;
  virtual void swapchainResize(uint32_t width, uint32_t height) = 0;
  virtual void draw() = 0;
  virtual void shutdown() = 0;
  virtual ~RHI(){};
};