#include <cstdint>
#include <map>
#include <memory>
#include <vector>

enum ShaderStages {
  VERTEX = 0,
  FRAGMENT = 1,
};

struct RenderPipelineInfo {
  std::map<ShaderStages, std::vector<char>> stages = {};
};

struct Pipeline {};

struct RenderPipeline : Pipeline {};

class RHI {
public:
  virtual std::shared_ptr<RenderPipeline>
  createRenderPipeline(RenderPipelineInfo &p_info) = 0;
  virtual void prepareFrame() = 0;
  virtual void setupViewport(float width, float height, float minDepth,
                             float maxDepth) = 0;
  virtual void setupScissor(int32_t x, int32_t y, uint32_t width,
                            uint32_t height) = 0;
  virtual void bindPipeline(std::shared_ptr<Pipeline> p_pipeline) = 0;
  virtual void draw(uint32_t vertexCount, uint32_t instanceCount,
                    uint32_t firstVertex, uint32_t firstInstance) = 0;
  virtual void drawIndexed() = 0;
  virtual void submit() = 0;
  virtual void freePipeline(std::shared_ptr<Pipeline> pipeline) = 0;
  virtual void shutdown() = 0;
  virtual ~RHI(){};
};