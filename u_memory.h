#ifndef U_MEMORY_HDR
#define U_MEMORY_HDR
#include <memory> // TODO: remove

namespace u {

template <typename T>
using unique_ptr = std::unique_ptr<T, std::default_delete<T>>;

}

#endif
