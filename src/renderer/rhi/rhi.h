#include <cstdint>
class RHI {
public:
  virtual void prepareFrame() = 0;
  virtual void draw() = 0;
  virtual void shutdown() = 0;
  virtual ~RHI(){};
};