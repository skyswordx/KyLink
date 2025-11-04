# IP-Messenger 协议 (FeiQ 变种) 实现验证的参考规范

## 1.0 基本原则与架构分析

本节分析协议的顶层设计决策，这些决策往往是实现中出现细微逻辑错误的根源。

### 1.1 双套接字架构：TCP/UDP 共享 2425 端口

IP-Messenger (IPMsg) 协议的一个核心特征是在单一端口 2425 上同时使用 UDP 和 TCP 1。UDP 用于用户发现（广播）、状态变更和即时消息传输；TCP 则专门用于文件和目录传输 5。

#### 逻辑推论与实现要求

应用程序的架构_必须_在同一个端口 (2425) 上绑定两个不同类型的套接字，并同时进行监听：

1. 一个 `SOCK_DGRAM` (UDP) 套接字：绑定到 `INADDR_ANY`（或特定接口）的 2425 端口。此套接字必须_永久_保持开放，以接收来自广播地址 (如 `255.255.255.255`) 和其他对等端（Unicast）的数据包。
    
2. 一个 `SOCK_STREAM` (TCP) 套接字：绑定到_相同_的端口和接口，并置于 `listen()` 状态，准备接受传入的连接请求。
    

#### 因果链与错误分析

这种双套接字模型是导致“细微逻辑问题”的主要来源。

- **事件链：**
    
    1. 客户端 A 通过 UDP 套接字向客户端 B 发送一个文件传输请求（包含文件元数据的 `IPMSG_SENDMSG`）。
        
    2. 客户端 B 的用户接受了该文件。
        
    3. 客户端 B（作为 TCP 客户端）向客户端 A 的 IP 和 2425 端口发起 TCP `connect()`。
        
    4. 客户端 A（作为 TCP 服务器）的 TCP 套接字上触发一个 `accept()` 事件。
        
- 潜在错误：
    
    如果应用程序在主线程或 I/O 线程中使用了阻塞式 (synchronous) recvfrom() 循环来处理 UDP 消息，该线程将永远无法处理 TCP 套接字上的 accept() 事件。这将导致所有文件传输请求因客户端 B 连接超时而失败。
    
- 解决原则（伪代码）：
    
    实现必须是异步的。这要求使用 I/O 多路复用 (I/O Multiplexing) 模型，例如 select(), poll(), epoll (Linux), kqueue (BSD/macOS) 或 IOCP (Windows)，或者使用一个线程池模型（例如：一个线程专用于 recvfrom()，一个线程专用于 accept()，并使用工作线程处理已建立的 TCP 连接），或者使用高级异步库 (如 Boost.Asio)。
    

代码段

```
// 异步 I/O 模型的逻辑骨架 (使用 select 示例)
function MainEventLoop():
    UdpSocket = CreateBoundUdpSocket(2425);
    TcpSocket = CreateBoundTcpListenSocket(2425);

    fd_set read_fds;
    while (AppIsRunning):
        FD_ZERO(read_fds);
        FD_SET(UdpSocket, read_fds);
        FD_SET(TcpSocket, read_fds);
        
        // 将所有活动的 TCP 连接套接字添加到 read_fds...
        //...

        // 阻塞，直到任一套接字上有活动
        select(max_fd + 1, read_fds, NULL, NULL, NULL);

        if (FD_ISSET(UdpSocket, read_fds)):
            HandleUdpPacket(UdpSocket);
        
        if (FD_ISSET(TcpSocket, read_fds)):
            // 接受新的文件传输连接
            NewTcpConnection = accept(TcpSocket,...);
            AddTcpConnectionToPool(NewTcpConnection);
        
        // 检查池中其他活动的 TCP 连接...
        //...
```

### 1.2 命令编号：位域 (Bitfield)，而非枚举 (Enumeration)

IPMsg 协议的 32 位 `CommandNo` (命令编号) 字段是一个_位域_，而不是一个简单的枚举值。这是一个极其重要的区分。

- **数据结构：** 32 位的 `CommandNo` 被分为两部分 6：
    
    - **低 8 位 (Bits 0-7):** 基础命令 (Base Command)，例如 `IPMSG_SENDMSG` (0x20) 5 或 `IPMSG_BR_ENTRY` (0x01) 5。
        
    - **高 24 位 (Bits 8-31):** 选项标志 (Option Flags)，例如 `IPMSG_FILEATTACHOPT` (0x00200000) 6 或 `IPMSG_SENDCHECKOPT` (0x00010000) 5。
        

#### 错误分析

- **潜在错误：** 您的代码可能使用了 `switch` 语句或 `if/else if` 链来检查_相等性_。
    
    代码段
    
    ```
    // 常见的“细微”错误逻辑
    function OnPacketReceived(CommandNo):
        if (CommandNo == IPMSG_SENDMSG): // 值为 0x20
            //... 处理消息
        else if (CommandNo == IPMSG_BR_ENTRY):
            //...
    
    // 当收到一个文件请求时 (CommandNo = IPMSG_SENDMSG | IPMSG_FILEATTACHOPT)，
    // 实际值为 0x00200020。
    // 上述所有 '==' 检查都将失败，导致该数据包被丢弃。
    ```
    
- 解决原则（伪代码）：
    
    在解析 CommandNo 时，必须始终使用位掩码 (Bitmasking) 将基础命令和选项标志分离。
    
    代码段
    
    ```
    // 正确的命令解析逻辑
    function OnPacketReceived(CommandNo):
        BaseCommand = CommandNo & 0xFF;     // 提取低 8 位基础命令
        Options = CommandNo & 0xFFFFFF00; // 提取高 24 位选项
    
        if (BaseCommand == IPMSG_SENDMSG):
            // 基础命令是“发送消息”
    
            if (Options & IPMSG_FILEATTACHOPT):
                // 这是一个文件传输请求
                HandleFileOffer(Packet);
            else:
                // 这是一个纯文本消息
                HandleTextMessage(Packet);
    
            if (Options & IPMSG_SENDCHECKOPT):
                // 发送方要求我们发送“收到”回执
                SendRecvConfirmation(Packet.PacketNo, Packet.SenderIP);
    
        else if (BaseCommand == IPMSG_BR_ENTRY):
            //...
    ```
    

### 1.3 数据包编号：时间戳作为事务 ID (Transaction ID)

协议中的 `PacketNo` (数据包编号) 字段是实现状态跟踪的核心。它不是一个简单的序列号，而是一个用于关联请求和响应的唯一事务 ID。

- **数据关联：**
    
    - **UDP-UDP:** 当 `IPMSG_SENDMSG` 包含 `IPMSG_SENDCHECKOPT` 标志时，接收方必须以 `IPMSG_RECVMSG` 作为响应，并且该响应的_附加信息_ (AdditionalSection) 必须包含原始的 `PacketNo` 5。
        
    - **UDP-TCP:** 当接收方接受一个 UDP 文件提议后，它会发起一个 TCP 连接，并发送 `IPMSG_GETFILEDATA` 命令。此 TCP 请求的_附加信息_必须包含原始 UDP 提议包的 `PacketNo` (此时称为 `packetID`) 5。
        

#### 错误分析

- **潜在错误：** 您的实现可能使用了一个简单的全局增量器 (如 `static unsigned int i = 0;... i++;`) 来生成 `PacketNo`。这是不可靠的，在程序重启后容易产生冲突，并且不符合其他客户端（如官方飞秋）的预期行为。
    
- 解决原则（伪代码）：
    
    PacketNo 应该是一个唯一的、(大部分情况下) 单调递增的 32 位数字。标准实现通常使用自某个时间点（例如午夜 UT 或系统启动）以来经过的毫秒数作为时间戳 7。
    
    代码段
    
    ```
    function GeneratePacketNo():
        // 返回自程序启动以来的毫秒数
        // C++11 示例:
        // auto now = std::chrono::steady_clock::now();
        // auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        // return (uint32_t)ms;
        return GetTimestampMilliseconds();
    ```
    

#### 架构要求：状态维护

由于 `PacketNo` 用于关联跨协议 (UDP->TCP) 的事务，应用程序_必须_维护一个状态表（哈希表或Map）。

- **必需的数据结构：** `std::map<uint32_t, FileOfferDetails>`
    
- **逻辑：**
    
    1. 当发送 UDP 文件提议 (步骤 3.2) 时，将 `PacketNo` 和文件详情 (如 `FileID`、本地路径) 存储在此 Map 中。
        
    2. 当在 TCP 套接字上收到 `IPMSG_GETFILEDATA` 请求 (步骤 3.4) 时，从其负载中解析出 `packetID`。
        
    3. 使用此 `packetID` 在 Map 中查找对应的 `FileOfferDetails`。
        
    4. 如果未找到，或 `FileID` 不匹配，则为无效请求，应关闭 TCP 连接。
        

### 1.4 负载 (Payload)：文本/二进制混合编码陷阱

协议的 `AdditionalSection` (附加信息) 字段是导致解析错误的最主要原因。它_不是_一个简单的 C 字符串。

- **数据结构分析：**
    
    1. **基础格式：** 协议在顶层是基于文本和冒号 (`:`) 分隔的 5。
        
    2. **混合格式：** _然而_，对于特定命令，`AdditionalSection` 包含由 `\0` (空字节, NULL) 分隔的多个子部分。
        
        - `IPMSG_BR_ENTRY` 和 `IPMSG_ANSENTRY` 的负载是：`Nickname` + `\0` + `GroupName` + `\0` 5。
            
        - `IPMSG_SENDMSG | IPMSG_FILEATTACHOPT` 的负载是：`MessageText` + `\0` + `FileMetadata` + `\0` 6。
            
    3. **编码：** 飞秋 (FeiQ) 是 IP-Messenger 的一个中国分支 9，它（以及许多早期亚洲软件）很可能使用 `GBK` 编码，而不是 `UTF-8` 10。
        
    4. **换行符：** 协议规范要求消息中的换行符应标准化为 UNIX 格式 (`0x0a`) 5。
        

#### 错误分析

- 潜在错误 1 (截断)： 您的代码可能在接收到 UDP 数据包后，直接将 AdditionalSection 的 C 指针 ( char* ) 转换为 std::string。
    
    std::string payload_str = std::string(packet->AdditionalSection);
    
    此操作将在第一个 \0 字节处停止，导致 GroupName 或 FileMetadata 信息被完全截断和丢失。
    
- **潜在错误 2 (GBK 编码陷阱)：** 您的代码可能正确地读取了完整长度的缓冲区 (例如 `std::string(buffer, length)` )，然后使用 `string::find(":")` 来分割文件元数据。
    
    - `GBK` 是一种多字节编码 10。
        
    - 冒号 (`:`) 的 ASCII 码是 `0x3A`。
        
    - 一个 `GBK` 编码的中文字符（例如文件名）的第二个字节_可能_恰好是 `0x3A`。
        
    - 在这种情况下，`find(":")` 会返回一个错误的位置，导致元数据解析完全失败。
        
- 解决原则（伪代码）：
    
    必须将所有传入的负载都视作原始的 char* 缓冲区和 length 来处理。
    
    代码段
    
    ```
    // 正确的负载解析逻辑
    function ParsePayload(buffer, length):
        // 1. 将缓冲区视为一个字节向量
        data = new Vector<byte>(buffer, length);
    
        // 2. 查找第一个 \0 分隔符
        null_pos = data.find('\0');
    
        primary_message_bytes = data.subVector(0, null_pos);
    
        // 3. 对文本部分进行编码转换
        // 错误根源：如果在 GBK 字节上直接执行 find(':') 会导致失败
        primary_message_string = ConvertFromGBK(primary_message_bytes); 
    
        // 4. 查找扩展数据 (如果存在)
        if (null_pos < length - 1):
            extended_data_bytes = data.subVector(null_pos + 1, data.length());
    
            // 示例: 文件元数据是 ASCII + GBK(文件名) 混合
            // 此时应使用更鲁棒的分割器，逐段解析
            // e.g., HandleFileMetadata(extended_data_bytes);
    
            // 示例: 组名
            // GroupName = ConvertFromGBK(extended_data_bytes.subVector(0, extended_data_bytes.find('\0')));
    ```
    

## 2.0 UDP 协议逻辑：在线状态与消息传输

本节详述用于核心功能（状态和消息）的 UDP 数据包交换工作流。

### 2.1 通用 UDP 数据包格式

所有 UDP 通信都遵循一个基于冒号分隔的文本格式 5。

**表 1：IPMsg v1 数据包结构**

|**字段**|**示例**|**描述**|**备注**|
|---|---|---|---|
|`Version`|`1`|协议版本号。必须为 '1'。|5|
|`PacketNo`|`1678886400`|唯一的事务 ID。|5。见原则 1.3 (应为时间戳)。|
|`SenderName`|`my_user`|发送方用户名 (登录名)。|5。GBK 编码 (见原则 1.4)。|
|`SenderHost`|`my_pc`|发送方主机名。|5。GBK 编码 (见原则 1.4)。|
|`CommandNo`|`262176`|32位命令字 (0x00040020)。|[5, 6]。位域 (见原则 1.2)。|
|`AdditionalSection`|`Hello\0Group1`|附加信息/负载。|5。格式随命令变化，可含 `\0`。|

#### 伪代码：数据包构造

代码段

```
// 构造一个 IPMSG 数据包
// (注意：此函数必须能处理 'additional_payload' 中嵌入的 \0 字节)
function CreatePacket(command_id, options, additional_payload):
    PacketNo = GeneratePacketNo(); // (来自 1.3)
    UserName = GetLocalUserName(); // 必须是 GBK 编码
    HostName = GetLocalHostName(); // 必须是 GBK 编码
    Command = command_id | options;
    
    // 格式: "1:PacketNo:UserName:HostName:Command:Payload"
    
    // 必须手动构建缓冲区，以确保 'additional_payload' 中的 \0 被包含在内，
    // 而不是被字符串格式化函数截断。
    
    PacketBuffer = new Buffer();
    PacketBuffer.Append("1:");
    PacketBuffer.Append(toString(PacketNo));
    PacketBuffer.Append(":");
    PacketBuffer.Append(UserName); // GBK
    PacketBuffer.Append(":");
    PacketBuffer.Append(HostName); // GBK
    PacketBuffer.Append(":");
    PacketBuffer.Append(toString(Command));
    PacketBuffer.Append(":");
    PacketBuffer.Append(additional_payload); // 这是一个包含 \0 的字节块
    
    return PacketBuffer;
```

### 2.2 用户发现工作流 (上线/下线)

#### 场景 1：客户端上线 (IPMSG_BR_ENTRY)

1. **动作 (客户端 A):** 启动。
    
2. **伪代码 (客户端 A):**
    
    代码段
    
    ```
    // 上线广播 
    function BroadcastEntry():
        // 负载格式: "Nickname\0GroupName\0"
        Nickname = GetLocalNickname(); // GBK 编码
        GroupName = GetLocalGroupName(); // GBK 编码
    
        PayloadBuffer = new Buffer();
        PayloadBuffer.Append(Nickname);
        PayloadBuffer.Append('\0');
        PayloadBuffer.Append(GroupName);
        PayloadBuffer.Append('\0');
    
        Packet = CreatePacket(IPMSG_BR_ENTRY, 0, PayloadBuffer);
    
        // 发送局域网广播
        UdpSocket.SendTo(Packet, "255.255.255.255", 2425);
    ```
    

#### 场景 2：现有客户端响应 (IPMSG_ANSENTRY)

1. **动作 (客户端 B):** 正在运行，收到来自 A 的 `IPMSG_BR_ENTRY` 包。
    
2. **伪代码 (客户端 B):**
    
    代码段
    
    ```
    // 在 OnUdpReceive 中
    function HandleUdpPacket(Packet, SenderIP):
        Parsed = ParsePacket(Packet);
    
        if (Parsed.BaseCommand == IPMSG_BR_ENTRY):
            // 1. 解析负载 (见原则 1.4)
            PayloadParts = SplitByNull(Parsed.AdditionalSection);
            Nickname = ConvertFromGBK(PayloadParts);
            GroupName = (PayloadParts.size() > 1)? ConvertFromGBK(PayloadParts) : "";
    
            // 2. 添加到用户列表
            UserList.AddOrUpdate(SenderIP, Parsed.SenderName, Parsed.SenderHost, Nickname, GroupName);
    
            // 3. 响应上线 (IPMSG_ANSENTRY)
            //    注意：这是 Unicast (单播) 回复给 SenderIP
            MyNickname = GetLocalNickname(); // GBK
            MyGroupName = GetLocalGroupName(); // GBK
    
            ResponsePayload = new Buffer();
            ResponsePayload.Append(MyNickname);
            ResponsePayload.Append('\0');
            ResponsePayload.Append(MyGroupName);
            ResponsePayload.Append('\0');
    
            ResponsePacket = CreatePacket(IPMSG_ANSENTRY, 0, ResponsePayload);
            UdpSocket.SendTo(ResponsePacket, SenderIP, 2425);
    ```
    

#### 场景 3：客户端下线 (IPMSG_BR_EXIT)

1. **动作 (客户端 A):** 关闭。
    
2. **伪代码 (客户端 A):**
    
    代码段
    
    ```
    // 下线广播 [12]
    function BroadcastExit():
        // 负载通常包含昵称
        Nickname = GetLocalNickname(); // GBK
        PayloadBuffer = new Buffer();
        PayloadBuffer.Append(Nickname);
        PayloadBuffer.Append('\0');
    
        Packet = CreatePacket(IPMSG_BR_EXIT, 0, PayloadBuffer);
    
        // 发送局域网广播
        UdpSocket.SendTo(Packet, "255.255.255.255", 2425);
    
        UdpSocket.Close();
        TcpListenSocket.Close();
    ```
    

### 2.3 消息传输工作流 (SENDMSG / RECVMSG)

#### 场景 1：客户端 A 发送消息 (IPMSG_SENDMSG)

1. **动作 (客户端 A):** 向 B 发送消息 "Hello"，并请求“已读回执”。
    
2. **伪代码 (客户端 A):**
    
    代码段
    
    ```
    function SendMessage(TargetIP, MessageText):
        // 1. 准备负载
        // 负载格式: "MessageText\0"
        MessageGBK = ConvertToGBK(MessageText);
        PayloadBuffer = new Buffer();
        PayloadBuffer.Append(MessageGBK);
        PayloadBuffer.Append('\0');
    
        // 2. 设置选项标志
        // IPMSG_SENDCHECKOPT (0x00010000) 请求对方发送 IPMSG_RECVMSG 
        Options = IPMSG_SENDCHECKOPT; 
    
        // 3. 创建并发送
        Packet = CreatePacket(IPMSG_SENDMSG, Options, PayloadBuffer);
        UdpSocket.SendTo(Packet, TargetIP, 2425);
    
        // 4. (关键) 记录此包，等待回执
        // 见原则 1.3
        PendingConfirmations.Add(Packet.PacketNo, {Timestamp: Now()});
    ```
    

#### 场景 2：客户端 B 接收并确认 (IPMSG_RECVMSG)

1. **动作 (客户端 B):** 收到来自 A 的 `IPMSG_SENDMSG` 包。
    
2. **伪代码 (客户端 B):**
    
    代码段
    
    ```
    // 在 HandleUdpPacket 中
    if (Parsed.BaseCommand == IPMSG_SENDMSG):
        // 1. 检查是否为文件 (见 3.0)
        if (Parsed.Options & IPMSG_FILEATTACHOPT):
            HandleFileOffer(Packet);
            return;
    
        // 2. 是纯文本消息。解析消息。
        // (注意：AdditionalSection 此时为 "MessageText\0")
        MessageText = ConvertFromGBK(Parsed.AdditionalSection.subVector(0, Parsed.AdditionalSection.find('\0')));
        DisplayMessage(Parsed.SenderName, MessageText);
    
        // 3. 检查是否需要回执 
        if (Parsed.Options & IPMSG_SENDCHECKOPT):
            // 对方要求了回执
            SendRecvConfirmation(Parsed.PacketNo, SenderIP);
    
    function SendRecvConfirmation(OriginalPacketNo, TargetIP):
        // 回执 (IPMSG_RECVMSG) 的负载是 *原始的 PacketNo* 
        // 这是一个字符串，不是数字
        Payload = toString(OriginalPacketNo) + '\0'; 
    
        ResponsePacket = CreatePacket(IPMSG_RECVMSG, 0, Payload);
        UdpSocket.SendTo(ResponsePacket, TargetIP, 2425);
    
    // 客户端 A 收到 RECVMSG 后的逻辑
    if (Parsed.BaseCommand == IPMSG_RECVMSG):
        // 负载是原始的 PacketNo
        OriginalPacketNo = stringToUInt32(Parsed.AdditionalSection.toString());
        if (PendingConfirmations.Contains(OriginalPacketNo)):
            PendingConfirmations.Remove(OriginalPacketNo);
            MarkMessageAsDelivered(OriginalPacketNo);
    ```
    

#### 错误分析

- **潜在错误 1：** 客户端 B _总是_ 发送 `IPMSG_RECVMSG`，即使 A 没有设置 `IPMSG_SENDCHECKOPT` 标志。这会造成不必要的网络流量。
    
- **潜在错误 2：** 客户端 B _从不_ 发送 `IPMSG_RECVMSG`。这违反了协议，导致客户端 A 永远等待确认，并可能（错误地）将消息标记为“发送失败” 5。
    
- **潜在错误 3：** 客户端 B 发送了 `IPMSG_RECVMSG`，但其负载_不是_原始的 `PacketNo`。客户端 A 收到回执，但在 `PendingConfirmations` 映射中找不到对应的条目，导致确认失败。
    

## 3.0 TCP 协议逻辑：文件与目录传输

这是协议中最复杂、最容易出错的部分。此工作流是异步的，并且涉及“反转”的客户端/服务器角色。

### 3.1 反转的客户端/服务器动态

- **数据分析：** 文件_发送方_ (Sender) 通过 UDP 广播一个“提议”(Offer) 6。文件_接收方_ (Receiver) 通过 TCP _主动连接_到发送方以下载文件 5。
    
- **逻辑推论与实现要求：**
    
    - **文件发送方 (例如客户端 A):** 必须扮演 **TCP 服务器** 角色。它必须在其 2425 端口上执行 `listen()` 和 `accept()` 。
        
    - **文件接收方 (例如客户端 B):** 必须扮演 **TCP 客户端** 角色。它必须 `connect()` 到客户端 A 的 IP 地址和 2425 端口。
        

### 3.2 工作流步骤 1：UDP 文件提议 (发送方 A)

1. **动作 (客户端 A):** 想要发送 `image.jpg` (1024 字节) 给客户端 B。
    
2. **数据格式：** 命令为 `IPMSG_SENDMSG | IPMSG_FILEATTACHOPT` 6。负载格式为 `MessageText` + `\0` + `FileMetadata` + `\0` 6。
    
3. 元数据格式 6： `FileID:FileName:Size(Hex):MTime(Hex):FileAttr(Hex)`。多个文件由 `\a` 分隔。
    
4. **伪代码 (客户端 A - 发送方/TCP服务器):**
    
    代码段
    
    ```
    function SendFileOffer(TargetIP, FilePath, MessageText):
        // 1. 生成文件元数据
        FileID = 0; // 包内唯一 ID，通常从 0 开始
        FileName = GetFileName(FilePath); // e.g., "image.jpg"
        FileSize = GetFileSize(FilePath); // e.g., 1024
        ModTime = GetFileModTime(FilePath); // e.g., 1678886500
        FileAttr = IPMSG_FILE_REGULAR; // (0x20) 
    
        // 2. 格式化元数据字符串
        // 注意：Size 和 MTime 必须是十六进制 [5, 6]
        // 注意：如果 FileName 包含 ':', 必须替换为 '::' 
        EscapedFileName = EscapeFileName(FileName); // (GBK 编码)
        MetadataString = format("%d:%s:%x:%x:%x", 
                                FileID, EscapedFileName, FileSize, ModTime, FileAttr);
    
        // (如果发送多个文件，用 \a 分隔它们 )
        // MetadataString += "\a" + format("%d:%s:...", FileID_2,...);
    
        // 3. 准备完整负载
        MessagePayload = ConvertToGBK(MessageText);
    
        PayloadBuffer = new Buffer();
        PayloadBuffer.Append(MessagePayload);
        PayloadBuffer.Append('\0');
        PayloadBuffer.Append(MetadataString);
        PayloadBuffer.Append('\0');
    
        // 4. 创建 UDP 包
        Options = IPMSG_FILEATTACHOPT | IPMSG_SENDCHECKOPT;
        Packet = CreatePacket(IPMSG_SENDMSG, Options, PayloadBuffer);
    
        // 5. (关键) 存储状态以备 TCP 请求
        // 见原则 1.3
        FileOfferDetails.Add(Packet.PacketNo, 
            { FileID_0: { Path: FilePath, ID: FileID }, 
              FileID_1:... });
        PendingConfirmations.Add(Packet.PacketNo,...);
    
        // 6. 发送 UDP 提议
        UdpSocket.SendTo(Packet, TargetIP, 2425);
    ```
    

### 3.3 工作流步骤 2：TCP 文件请求 (接收方 B)

1. **动作 (客户端 B):** 收到 UDP 提议，用户点击“接受”。
    
2. **伪代码 (客户端 B - 接收方/TCP客户端):**
    
    代码段
    
    ```
    // 在 HandleUdpPacket -> HandleFileOffer 中调用
    function OnFileOfferAccepted(OriginalUdpPacket, SenderIP):
        // 1. 解析元数据 (来自 UDP 负载)
        Payloads = SplitByNull(OriginalUdpPacket.AdditionalSection);
        Message = ConvertFromGBK(Payloads);
        MetadataString = Payloads;
    
        // 假设只请求第一个文件
        FileMeta = ParseFileMetadata(MetadataString.split('\a')); 
        // FileMeta = { FileID: 0, Name: "image.jpg", Size: 1024 }
    
        // 2. 创建 TCP 套接字并连接 (扮演 TCP 客户端)
        TcpClientSocket = new Socket(TCP);
        TcpClientSocket.Connect(SenderIP, 2425); // 
    
        // 3. 准备 TCP 请求负载 
        // 格式: "PacketID(Hex):FileID(Hex):Offset(Hex)"
        OriginalPacketNo_Hex = format("%x", OriginalUdpPacket.PacketNo);
        FileID_Hex = format("%x", FileMeta.FileID);
        Offset_Hex = "0";
    
        TcpPayload = OriginalPacketNo_Hex + ":" + FileID_Hex + ":" + Offset_Hex + '\0';
    
        // 4. (关键) TCP 请求本身也是一个完整的 IPMSG 包
        //    使用 IPMSG_GETFILEDATA 命令 
        TcpRequestPacket = CreatePacket(IPMSG_GETFILEDATA, 0, TcpPayload);
    
        // 5. 通过 TCP 发送请求包
        TcpClientSocket.Send(TcpRequestPacket);
    
        // 6. 准备接收 *原始* 文件数据
        FileHandle = OpenFileForWriting(FileMeta.Name);
        ReceiveFileLoop(TcpClientSocket, FileHandle, FileMeta.Size);
    
    // (续)
    function ReceiveFileLoop(Socket, FileHandle, TotalSize):
        BytesReceived = 0;
        while (BytesReceived < TotalSize):
            // (关键) 接收的数据是 *原始文件字节*
            // 此处 *没有* IPMSG 包头或格式 
            DataChunk = Socket.Receive(BufferSize); 
    
            if (DataChunk.IsEmpty):
                // 对方提前关闭了连接（错误）
                break; 
    
            FileHandle.Write(DataChunk);
            BytesReceived += DataChunk.Length;
    
            // 确保不多也不少
            if (BytesReceived > TotalSize):
                // 协议错误
                break;
    
        FileHandle.Close();
        Socket.Close();
    ```
    

### 3.4 工作流步骤 3：TCP 数据响应 (发送方 A)

1. **动作 (客户端 A):** 在其 TCP 监听套接字上 `accept()` 来自 B 的连接。
    
2. **伪代码 (客户端 A - 发送方/TCP服务器):**
    
    代码段
    
    ```
    // 在主事件循环的 'accept' 之后调用 (见 1.1)
    function OnTcpConnectionAccepted(TcpServerConnection):
        // 1. (关键) 从 TCP 连接中读取 *IPMSG 请求包*
        //    这 *不是* 原始数据流的开始
        RequestPacketBuffer = TcpServerConnection.Receive(MaxPacketSize);
        ParsedRequest = ParsePacket(RequestPacketBuffer);
    
        if (ParsedRequest.BaseCommand == IPMSG_GETFILEDATA):
            // 2. 解析 TCP 请求负载
            // 格式: "PacketID(Hex):FileID(Hex):Offset(Hex)" 
            RequestPayload = ParsedRequest.AdditionalSection.toString();
            Params = RequestPayload.split(':');
    
            PacketID_Hex = Params;
            FileID_Hex = Params;
            Offset_Hex = Params;
    
            // 3. 转换并验证
            OriginalPacketNo = ConvertFromHex(PacketID_Hex);
            FileID = ConvertFromHex(FileID_Hex);
            Offset = ConvertFromHex(Offset_Hex);
    
            // 4. (关键) 检查状态表 (见 1.3)
            if (FileOfferDetails.Contains(OriginalPacketNo)):
                Offer = FileOfferDetails.Get(OriginalPacketNo);
    
                // 检查请求的 FileID 是否在提议中
                if (Offer.ContainsFileID(FileID)):
                    FilePath = Offer.GetFilePath(FileID);
    
                    // 5. 验证通过。开始发送 *原始文件字节*
                    FileHandle = OpenFileForReading(FilePath);
                    FileHandle.Seek(Offset);
    
                    while (not FileHandle.EOF()):
                        // (关键) 发送原始数据块，*不带任何* IPMSG 格式 
                        DataChunk = FileHandle.Read(BufferSize);
                        SendResult = TcpServerConnection.Send(DataChunk);
                        if (SendResult == FAILED):
                            break; // 客户端断开连接
    
                    FileHandle.Close();
                else:
                    // 错误：无效的 FileID
                    Log("Error: Invalid FileID requested.");
            else:
                // 错误：无效的 PacketNo (未知的事务)
                Log("Error: Invalid PacketNo for file transfer.");
    
        // 5. 发送完成或出错，关闭此 TCP 连接
        TcpServerConnection.Close();
    ```
    

### 3.5 特殊情况：目录传输 (IPMSG_GETDIRFILES)

目录传输遵循一个更复杂的递归逻辑。

1. **UDP 提议：** 发送方 A 在 `FileAttr` 字段使用 `IPMSG_FILE_DIR` (0x10) 标志 6。
    
2. **TCP 请求：** 接收方 B _不_ 发送 `IPMSG_GETFILEDATA`，而是发送 `IPMSG_GETDIRFILES` 5。其负载格式与 `GETFILEDATA` 相同 (`PacketID:FileID:Offset`)。
    
3. **TCP 响应：** 发送方 A _不_ 回复原始数据。相反，它回复一个_格式化的文本列表_，描述该目录的全部内容 5。
    
    - 响应格式 5：`header-size:filename:file-size:fileattr:...:contents-data...`
        
    - `IPMSG_FILE_DIR` (0x10) 表示子目录。
        
    - `IPMSG_FILE_REGULAR` (0x20) 表示文件。
        
    - `IPMSG_FILE_RETPARENT` (0x01) 表示返回父目录。
        
4. **递归：** 接收方 B 解析此列表。
    
    - 对于列表中的每个 `IPMSG_FILE_REGULAR` 条目，B 必须发起一个_新_的 `IPMSG_GETFILEDATA` TCP 请求（如 3.3 所示）来下载该文件。
        
    - 对于列表中的每个 `IPMSG_FILE_DIR` 条目，B 必须发起一个_新_的 `IPMSG_GETDIRFILES` TCP 请求，以递归地获取该子目录的内容。
        

## 4.0 数据结构与命令总结 (参考表)

### 表 2：命令字 (CommandNo) 位域解析

此表区分了基础命令（互斥）和选项标志（可组合）。实现必须使用位掩码 (如 1.2 所述) 来解析。

|**类型**|**名称**|**十六进制值**|**位位置 (估)**|**描述**|
|---|---|---|---|---|
|基础|`IPMSG_NOOPERATION`|`0x00000000`|0|无操作 (No Operation) 6|
|基础|`IPMSG_BR_ENTRY`|`0x00000001`|0|广播：客户端上线 5|
|基础|`IPMSG_BR_EXIT`|`0x00000002`|1|广播：客户端下线 [12]|
|基础|`IPMSG_ANSENTRY`|`0x00000003`|2|单播：响应 `BR_ENTRY` 5|
|基础|`IPMSG_SENDMSG`|`0x00000020`|5|单播：发送消息 5|
|基础|`IPMSG_RECVMSG`|`0x00000021`|5|单播：确认收到 `SENDMSG` 5|
|基础|`IPMSG_GETFILEDATA`|`0x00000060`|6|TCP 请求：请求文件数据 5|
|基础|`IPMSG_GETDIRFILES`|`0x00000061`|6|TCP 请求：请求目录内容 5|
|选项|`IPMSG_ABSENCEOPT`|`0x00000100`|8|标志：客户端处于离开状态 (用于 `BR_ENTRY`) 6|
|选项|`IPMSG_SENDCHECKOPT`|`0x00010000`|16|标志：发送方请求 `RECVMSG` 确认 5|
|选项|`IPMSG_FILEATTACHOPT`|`0x00200000`|21|标志：`SENDMSG` 包含文件元数据 6|

### 表 3：文件元数据负载格式 (用于 `IPMSG_FILEATTACHOPT`)

此表描述了 `IPMSG_SENDMSG` 命令的 `AdditionalSection` 的复杂结构 6。

- **顶层结构：** `MessageText` + `\0` + `FileMetadataBlock` + `\0`
    
- **`FileMetadataBlock` 结构：** `File1` + `\a` + `File2` + `\a` +...
    
- `FileN` 结构 6： `fileID:filename:size:mtime:fileattr[:extend-attr...]`
    

|**字段**|**格式**|**示例**|**描述**|
|---|---|---|---|
|`fileID`|十进制 (字符串)|`0`|此数据包内的 0 索引文件 ID 6。|
|`filename`|字符串 (GBK)|`文件.txt`|文件名。冒号 (`:`) 必须转义为 `::` 6。|
|`size`|十六进制 (字符串)|`A0`|文件大小 (字节)。(示例为 160 字节) 6。|
|`mtime`|十六进制 (字符串)|`64105E0A`|文件的最后修改时间 (UNIX 时间戳) 6。|
|`fileattr`|十六进制 (字符串)|`20`|文件属性。`0x20` (文件) 或 `0x10` (目录) 6。|
|`extend-attr`|字符串|(可选)|扩展属性 (较新版本使用)。|

## 5.0 针对“细微逻辑错误”的调试清单

此清单旨在帮助 AI 代理或开发人员验证现有 C++ 实现中的常见逻辑缺陷。

1. **错误源：异步 I/O (见 1.1)**
    
    - **问题：** 应用程序是否能在_同一时刻_准备好接收 UDP 包 _和_ 接受新的 TCP 连接？
        
    - **检查：** 检查主 I/O 线程中是否存在任何_阻塞_的 `recvfrom()` 或 `recv()` 调用。如果 UDP 接收循环（即使在单独的线程中）没有正确地与 TCP `accept()` 协调（例如通过 `select`/`epoll`），文件传输将 100% 失败。
        
2. **错误源：命令解析 (见 1.2)**
    
    - **问题：** 您的代码是否将 32 位命令字视为位域？
        
    - **检查：** 查找代码中的 `if (command == IPMSG_SENDMSG)`。这是_错误_的。它必须是 `if ((command & 0xFF) == IPMSG_SENDMSG)`。随后必须_单独_检查 `if (command & IPMSG_FILEATTACHOPT)`。
        
3. **错误源：编码与负载解析 (见 1.4)**
    
    - **问题：** 您的代码是否正确处理了负载中的 `\0` 字节和 `GBK` 编码？
        
    - **检查：** 查找将 `AdditionalSection` 的 `char*` 指针直接赋给 `std::string` 的代码。这是_错误_的，它会截断数据。必须使用 `std::string(buffer, length)`。
        
    - **检查：** 查找 `IPMSG_BR_ENTRY` 的解析器。它是否在第一个 `\0` _之后_ 提取了 `GroupName`？5。
        
    - **检查：** 查找 `IPMSG_FILEATTACHOPT` 的解析器。它是否在第一个 `\0` _之后_ 提取了 `FileMetadata`？6。
        
    - **检查：** 在对文件名执行冒号 (`:`) 分割_之前_，是否已将文件名从 `GBK` 转换为 `UTF-8`（或 `wchar_t`）？在原始 `GBK` 字节上执行 `find(":")` 是不可靠的 10。
        
4. **错误源：TCP 角色反转 (见 3.1)**
    
    - **问题：** 谁在发起 TCP 连接？
        
    - **检查：** 文件_接收方_必须调用 `connect()`。文件_发送方_必须调用 `accept()`。如果您的代码逻辑相反，连接将永远无法建立。
        
5. **错误源：事务状态 (见 1.3 & 3.4)**
    
    - **问题：** 您的 TCP 服务器（文件发送方）如何知道传入的 TCP 请求对应哪个文件？
        
    - **检查：** 您的代码_必须_在发送 UDP 提议时，将 `PacketNo` 存储在一个 `std::map` 中。当 TCP 请求（`IPMSG_GETFILEDATA`）到达时，它会在其负载中包含这个（十六进制编码的）`PacketNo` 5。您的代码必须查询此 Map 来查找要发送的文件路径。如果 UDP `PacketNo` 在发送后即被丢弃，则 TCP 验证将失败。
        
6. **错误源：数据流模式 (见 3.3 & 3.4)**
    
    - **问题：** TCP 服务器（发送方）在验证 `GETFILEDATA` 请求后发送了什么？
        
    - **检查：** 它必须发送_纯粹的、原始的文件字节数据_，并且_仅此而已_ 5。它_不得_发送任何 IPMSG 包头或格式。接收方必须精确读取 UDP 元数据中指定的字节数（`Size`）6，然后关闭连接。