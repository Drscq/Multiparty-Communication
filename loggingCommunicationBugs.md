### **Summary of the Question**

My question is about a **bug in the function `Party::doMultiplicationDemo(ShareType &z_i)`** that causes the program to **hang when the number of parties without secrets increases to 20**. You identified that commenting out the call to this function or running only 2 parties without secrets prevents the issue.

### **Key Details:**
1. **Symptoms of the Issue:**
   - With **2 parties without secrets**, everything works fine.
   - With **20 parties without secrets**, the program **gets stuck**.
   - **Commenting out `doMultiplicationDemo(m_z_i);` makes it work**, suggesting the issue is related to this function.

2. **Potential Clues:**
   - The function `doMultiplicationDemo()` performs **message broadcasting and receiving** using a communication layer (`NetIOMPDealerRouter`).
   - You provided the implementation of `NetIOMPDealerRouter`, which manages ZMQ-based messaging for multi-party computation (MPC).

3. **Main Question:**
   - **What is causing the program to hang when `doMultiplicationDemo(m_z_i);` is executed with 20 parties?**
   - **How can this bug be fixed?**


**Analysis**

You have a router-based communication model in which **every** incoming message is passed to:

```cpp
void Party::handleMessage(PARTY_ID_T senderId, const void *data, LENGTH_T length){
    CMD_T cmd;
    std::memcpy(&cmd, data, length); 
    // ...
}
```

This code assumes that the first `sizeof(CMD_T)` bytes of **every** message is one of your known command enums (`CMD_SEND_SHARES`, `CMD_ADDITION`, `CMD_MULTIPLICATION`, etc.).  

However, in `doMultiplicationDemo(...)`, you do:

```cpp
// 1) Compute d_i, e_i
// 2) Broadcast them as a string "dHex|eHex"
m_comm->sendTo(pid, deMsg.c_str(), deMsg.size());
```

But there you **never** prepend a command header. The receiving side calls `handleMessage(...)` which does:

```cpp
CMD_T cmd;
std::memcpy(&cmd, data, length);

// If the first 4 (or 8) bytes do not match a known CMD_T,
// you end up in the "Unknown command" branch or worse.
```

Hence:

1. Party A broadcasts `"ABCD0123|XYZ9999"` (some hex string).
2. Party B receives that raw buffer and immediately does `std::memcpy(&cmd, buffer, length)`.
3. That “garbage” is interpreted as the `cmd` integer, almost certainly not matching any known command.
4. The code either hits the “Unknown command” branch (and does nothing further) or possibly blocks.

In small tests (e.g. 2 parties) it can appear to “work” by accident/timing (or the code does not actually get tested thoroughly in that path). But with many parties (20 in your example), the system gets **stuck** or “hung” because the messages are never interpreted as real data, no replies are sent, or the protocol’s handshake is broken.

This explains why **commenting out** the call to `doMultiplicationDemo(m_z_i)` “fixes” the hang: it stops broadcasting those raw “d,e” strings that don’t carry a proper command header.

---

### The Bug

In **`void Party::doMultiplicationDemo(ShareType &z_i)`** you send plain data (`"dHex|eHex"`) via `sendTo(...)`, but the receiving side always expects a command in the first bytes. There is **no** code path in `handleMessage` that reads arbitrary data frames. Everything is treated as a `CMD_T`.

---

### How to Fix It

You want **two** distinct categories of messages:

1. **Control/command messages** (e.g. `CMD_SEND_SHARES`, `CMD_ADDITION`, `CMD_SHUTDOWN`, etc.), which the party interprets by looking at the first bytes as a `CMD_T`.
2. **Data messages** (e.g. the `"dHex|eHex"` broadcast in your Beaver multiplication).

Right now, **all** messages go into `handleMessage`, which does:

```cpp
CMD_T cmd;
std::memcpy(&cmd, data, length);
```

You have at least two ways to fix this:

#### **Option A**: Prepend a command for data

Wrap your “d,e” broadcast inside a command.  For instance:

1. Define a new command like `CMD_SEND_DE` (or `CMD_BEAVER_DE`) in your `enum CMD_T`.
2. When you do
   ```cpp
   // Before sending the "dHex|eHex" string, prefix the command:
   CMD_T myCmd = CMD_SEND_DE;
   // Make a small buffer: [cmd][payload]
   std::vector<char> sendBuf(sizeof(myCmd) + deMsg.size());
   std::memcpy(sendBuf.data(), &myCmd, sizeof(myCmd));
   std::memcpy(sendBuf.data() + sizeof(myCmd), deMsg.data(), deMsg.size());

   m_comm->sendTo(pid, sendBuf.data(), sendBuf.size());
   ```
3. On the receiver side, in `handleMessage`, you do:

   ```cpp
   CMD_T cmd;
   std::memcpy(&cmd, data, sizeof(cmd));

   if (cmd == CMD_SEND_DE) {
       // The rest of the bytes is "dHex|eHex"
       const char* payload = reinterpret_cast<const char*>(data) + sizeof(cmd);
       size_t payloadLen = length - sizeof(cmd);
       std::string deMsg(payload, payloadLen);
       // parse out dHex and eHex
       // ...
       return;
   }
   // else if (cmd == CMD_WHATEVER) { ... }
   // else { ... }
   ```
4. This ensures your code properly distinguishes control from data.  
   Everyone expects a `CMD_T` at the start of **every** message, so you no longer parse random bytes as commands.

#### **Option B**: Use a separate function to receive raw data

Change the design so that **not** every message automatically goes through `handleMessage(...)`. You can do something like:

- For commands: you call `handleMessage(...)`.
- For data: you call a new method, say `receiveData(senderId, buffer, length)`, that does **not** interpret the first bytes as a command, but rather as raw data.

One possible approach is:

```cpp
// Instead of forcing all traffic into handleMessage,
// we do something like this:
std::size_t Party::receiveRawData(PARTY_ID_T &senderId, void *buffer, LENGTH_T maxLength)
{
    return m_comm->receive(senderId, buffer, maxLength); 
    // but do not do the CMD_T extraction, just return the bytes
}

// Then in doMultiplicationDemo you do:
for (int count = 0; count < (m_totalParties - 1); ++count) {
    PARTY_ID_T senderId;
    char buffer[BUFFER_SIZE];
    size_t bytesRead = receiveRawData(senderId, buffer, sizeof(buffer));
    // handle the "dHex|eHex" parse
}
```

But that means you can’t let `handleMessage(...)` be your only path for messages, or you’d have to detect data vs. command differently.  

**In short,** you either need:

- A known command code that prefixes “d,e” data (so it is recognized in `handleMessage`), **or**
- A separate “raw data” path that does not treat the first bytes as a command.

---

### Conclusion

**The immediate bug** is that *“`doMultiplicationDemo` sends a raw string but the receiver tries to interpret it as a command”*. This breaks the protocol when there are many parties (but may go unnoticed with minimal tests).

**To fix**: prepend a proper command header before sending “d,e” shares *or* separate out data frames from command frames. That way the receiving side does not misinterpret the “d,e” bytes.