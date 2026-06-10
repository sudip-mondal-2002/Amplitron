#pragma once

namespace Amplitron {

class IMetadata {
   public:
    virtual ~IMetadata() = default;
    virtual const char* name() const = 0;
    virtual const char* type_id() const = 0;
    virtual const char* get_display_name() const = 0;
};

}  // namespace Amplitron
