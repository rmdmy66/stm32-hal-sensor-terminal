# 多通道数据采集终端（4 周 MVP）

目标：用一块 `STM32F103C8T6` 最小系统板、`ST-Link V2`、`USB-TTL` 和几个现成模块，在 3 周内做出可演示版本，第 4 周只做打磨和补测。全程不焊接、不画 PCB。

## 项目目标与验收价值

- 你能把一条数据链完整打通：采集 -> 处理 -> 传输 -> 显示/存储。
- 你能处理异常：传感器掉线、CRC 错帧、队列满、任务卡死。
- 你能用数字说话：采样率、丢帧数、24 小时重启次数、栈余量、CPU 占用。

## 范围

必做：

- `ADC + 定时器触发 + DMA` 双缓冲，1 kHz 采样。
- `SHT30` 温湿度，I2C 读取，带超时和重试。
- `FreeRTOS` 多任务：采集、处理、通信、显示、监控。
- 自定义帧协议：帧头、长度、命令、序号、CRC16、帧尾。
- Python 上位机：解析、打印、存 CSV、实时曲线。
- 独立看门狗、错误计数、24 小时老化测试。

加分项：

- OLED 显示。
- 内部 Flash 参数存储。
- STOP 模式 + IDD 电流测量。
- W25Q64 日志。
- Modbus RTU 或 OTA。

不做：

- PCB 设计、焊接。
- 复杂外壳、电池管理。
- 多传感器融合、机器学习。

## 硬件

详见 `docs/BOM.md`。已有主控板、ST-Link 和 USB-TTL，新增模块预算约 80-130 元。

## 软件架构

```text
 SHT30 ──I2C──┐
 电位器 ─ADC──┤
              ▼
        ┌────────────────┐    USART1/USB-TTL   ┌──────────────┐
        │ STM32F103C8T6  │ ─────────────────► │ Python 上位机 │
        │ HAL + FreeRTOS │ ◄───────────────── │ 解析 / 曲线   │
        └──────┬─────────┘                    └──────────────┘
               │
          ┌────┴────┐
          │  OLED   │
          └─────────┘
```

任务划分（目标）：

| 任务 | 优先级 | 周期 | 职责 |
|---|---|---|---|
| `acq_task` | 高 | 1 ms | 启动 ADC，读 SHT30（1 Hz） |
| `proc_task` | 中 | 事件 | 滤波、换算、打包 |
| `comm_task` | 中 | 事件 | 串口发送、协议解析、ACK |
| `disp_task` | 低 | 200 ms | OLED 刷新 |
| `mon_task` | 低 | 1 s | 喂狗、统计、打印 |

## 目录

```text
docs/                 需求、物料、计划、协议、开发指南
firmware/protocol/    与硬件无关的纯 C 协议层
firmware/app/         应用层（后面放 FreeRTOS 任务）
firmware/bsp/         板级驱动（后面放 HAL 封装）
host/                 Python 上位机
test/                 主机侧测试和语法检查
hardware/             接线图、引脚表
```

项目文档：

- `docs/BOM.md`：物料清单与引脚计划
- `docs/plan.md`：四周开发计划与验收指标
- `docs/protocol.md`：串口帧协议与 CRC16
- `docs/week0.md`：F103 + HAL 第一课
- `hardware/wiring.md`：ST-Link、USB-TTL、传感器接线
- `docs/git_github_guide.md`：Git 日常提交与 GitHub 推送

## 当前进度

- [x] 项目骨架
- [x] 物料清单
- [x] 四周计划
- [x] 协议帧格式和纯 C 实现
- [x] Python 对照解析器
- [x] 确定主控方案：STM32F103C8T6 + HAL + ST-Link V2 + USB-TTL
- [ ] 完成第 0 课：点灯 + 串口日志 + HardFault

## 下一步

1. 按 `docs/week0.md` 完成 F103 的 CubeMX 工程、LED、USART1 日志和 HardFault。
2. 按 `hardware/wiring.md` 接好 ST-Link、USB-TTL 和电源，先不要接传感器。
3. 运行 `python host/frame_tool.py --self-test`，先熟悉协议。
