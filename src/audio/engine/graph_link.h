#pragma once

namespace Amplitron {

struct GraphLink {
  int id = 0;
  int source_pin_id = 0; // Mapping from Output Pin
  int dest_pin_id = 0;   // Mapping to Input Pin
};

} // namespace Amplitron
