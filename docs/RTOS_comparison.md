| OS Name           | TCP/UDP Support | Event-driven Network API | Min RAM (Networking) | Notes                                         |
|-------------------|-----------------|-------------------------|----------------------|-----------------------------------------------|
| **FreeRTOS**      | Yes (FreeRTOS+TCP, lwIP) | No native `select`/`poll`; lwIP offers `select()` | ~8–32 KB            | TCP stack needs ~8KB, lwIP ~16KB+             |
| **Zephyr**        | Yes (builtin stack)      | Partial: Zephyr sockets have `poll()`; not full POSIX | ~32–64 KB            | TLS increases RAM need                        |
| **RIOT OS**       | Yes (GNRC, lwIP)         | GNRC: event/callback based; lwIP: `select()` | ~32 KB                | lwIP: `select()`, GNRC: msg/callback          |
| **Mbed OS**       | Yes (builtin stack)      | No POSIX `select`/`poll`; event/callback APIs | ~32–64 KB             | TLS increases RAM need                        |
| **ChibiOS**       | Yes (lwIP)               | lwIP provides `select()`                      | ~16–32 KB               | With lwIP, up to 32KB typical                 |
| **NuttX**         | Yes (builtin stack)      | Yes: `select()`, `poll()`, POSIX sockets      | ~32–64 KB              | Most POSIX-compliant, RAM depends on config   |
| **ThreadX (Azure RTOS)** | Yes (NetX Duo)   | No POSIX; NetX Duo has callback/event APIs    | ~24–64 KB               | Source-available, RAM scales w/features       |

---

### **Event-driven APIs Explained**
- **POSIX-like (`select`, `poll`)**: NuttX, lwIP (used by FreeRTOS, RIOT, ChibiOS).
- **Custom event/callback APIs**: Zephyr, RIOT (GNRC), Mbed OS, ThreadX.
- **Advanced APIs (epoll, kevent, IOCP)**: **None** of these embedded OSes natively support advanced scalable I/O APIs like `epoll` (Linux), `kevent` (BSD), or `IOCP` (Windows), as these are not typical for MCUs.

---

### **RAM Notes**
- The minimum RAM depends on stack configuration (TCP only, TCP+UDP, number of sockets, buffer sizes).
- For TLS/SSL (e.g., mbedTLS), RAM requirements can double or triple.

---

**Summary:**  
- All listed OSes support TCP/UDP.
- POSIX-style network event APIs (`select`, `poll`) are available primarily in **NuttX** and when using **lwIP** with other OSes.
- Minimum RAM usage with networking ranges from **~8 KB (FreeRTOS minimal TCP)** to **~64 KB (full-featured stacks with TLS)**.
- 