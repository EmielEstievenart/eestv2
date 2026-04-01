# TCP Connection Renaming Refactor

## Summary

All connection-related classes have been renamed to include the "Tcp" prefix for consistency, as the `TcpServer` class already used this naming convention.

## Changes Made

### File Renames

| Old Filename | New Filename |
|--------------|--------------|
| `connection.hpp` | `tcp_connection.hpp` |
| `client_connection.hpp` | `tcp_client_connection.hpp` |
| `server_connection.hpp` | `tcp_server_connection.hpp` |

### Class Renames

| Old Class Name | New Class Name |
|----------------|----------------|
| `Connection` | `TcpConnection` |
| `ClientConnection` | `TcpClientConnection` |
| `ServerConnection` | `TcpServerConnection` |

*(Note: `TcpServer` was already named correctly and remains unchanged)*

## Rationale

**Before:** Naming was inconsistent - connection classes (`Connection`, `ClientConnection`, `ServerConnection`) did not include protocol information, while `TcpServer` did.

**After:** All classes now consistently include "Tcp" prefix, making it clear they are TCP-specific implementations and maintaining consistency throughout the API.

## Updated Files

### Source Code
- ✅ `code/eestv/net/tcp_connection.hpp` (created, replaces `connection.hpp`)
- ✅ `code/eestv/net/tcp_client_connection.hpp` (created, replaces `client_connection.hpp`)
- ✅ `code/eestv/net/tcp_server_connection.hpp` (created, replaces `server_connection.hpp`)
- ✅ `code/eestv/net/tcp_server.hpp` (updated includes and references)

### Tests
- ✅ `tests/unit_tests/net/tcp_connection_tests.cpp`
- ✅ `tests/unit_tests/net/keep_alive_callback_tests.cpp`

### Examples
- ✅ `examples/net/tcp_connection/main.cpp`

### Documentation
- ✅ `docs/eestv/net.md`
- ✅ `docs/eestv/net_buffer_interface.md`
- ✅ `docs/eestv/net_keep_alive_callback.md`
- ✅ `docs/eestv/net_connection_architecture.md` (if exists)

## Migration Guide

For users of the library, update your code as follows:

### Include Statements

```cpp
// Old
#include "eestv/net/connection.hpp"
#include "eestv/net/client_connection.hpp"
#include "eestv/net/server_connection.hpp"

// New
#include "eestv/net/tcp_connection.hpp"
#include "eestv/net/tcp_client_connection.hpp"
#include "eestv/net/tcp_server_connection.hpp"
```

### Class Usage

```cpp
// Old
std::shared_ptr<eestv::Connection<>> base_conn;
std::shared_ptr<eestv::ClientConnection<>> client_conn;
std::shared_ptr<eestv::ServerConnection<>> server_conn;

// New
std::shared_ptr<eestv::TcpConnection<>> base_conn;
std::shared_ptr<eestv::TcpClientConnection<>> client_conn;
std::shared_ptr<eestv::TcpServerConnection<>> server_conn;
```

### Complete Example

```cpp
// Old
#include "eestv/net/client_connection.hpp"

auto client = std::make_shared<eestv::ClientConnection<>>(
    endpoint, io_context, std::chrono::seconds(5));

// New
#include "eestv/net/tcp_client_connection.hpp"

auto client = std::make_shared<eestv::TcpClientConnection<>>(
    endpoint, io_context, std::chrono::seconds(5));
```

## Benefits

1. **Consistency**: All TCP networking classes now follow the same naming convention
2. **Clarity**: The "Tcp" prefix makes it immediately clear these are TCP-specific implementations
3. **Future-Proofing**: If UDP or other protocol connections are added later, the distinction will be clear
4. **Professional**: Consistent naming improves code readability and maintainability

## Testing

✅ Library compiles successfully  
✅ All references updated in:
  - Source code
  - Test files
  - Example code
  - Documentation

## Backward Compatibility

⚠️ **Breaking Change**: This is a breaking change. Users will need to update their include statements and class names.

The old files have been removed, so code using the old names will not compile until updated.
