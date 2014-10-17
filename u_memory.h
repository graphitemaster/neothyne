#ifndef U_MEMORY_HDR
#define U_MEMORY_HDR
#include <memory>

namespace u {
    template <typename T, typename D = std::default_delete<T>>
    using unique_ptr = std::unique_ptr<T, D>;
}

#endif
