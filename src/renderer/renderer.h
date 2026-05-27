
enum RendererBackend {
  NONE = 0,
  VULKAN = 1,
};

class GLFWwindow;
class RHI;

class Renderer {
public:
  Renderer(RendererBackend backend, GLFWwindow *window = nullptr);

  void update();
  void shutdown();

private:
  GLFWwindow *window = nullptr;
  RHI *rhi = nullptr;
};