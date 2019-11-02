#include <afina/Version.h>

#include <string>
#include <sstream>

namespace Afina {

const std::string Version_Major = "0";
const std::string Version_Minor = "0";
const std::string Version_Patch = "0";
const std::string Version_Release = "";
const std::string Version_SHA =  "g3a59";

std::string get_version() {
  std::stringstream app_string;
  app_string << "Afina " << Afina::Version_Major << "." << Afina::Version_Minor << "." << Afina::Version_Patch;
  if (Afina::Version_SHA.size() > 0) {
      app_string << "-" << Afina::Version_SHA;
  }
  return app_string.str();
}

} // namespace Afina
