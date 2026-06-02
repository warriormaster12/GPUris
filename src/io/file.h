
#include <string>
#include <vector>

class File {
public:
  static std::vector<char> read(const std::string &filename);
};