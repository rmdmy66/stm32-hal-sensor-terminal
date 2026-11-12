# 第 0 课：STM32F103C8T6 + HAL，从空白到“能看见日志”

这一课不写业务，只做四件事：能下载、能点灯、能看串口、能定位 HardFault。做完再碰传感器和 FreeRTOS。

## 1. 准备

你需要：

- `STM32F103C8T6` 最小系统板
- `ST-Link V2`
- `USB-TTL` 模块（CH340 或 CP2102）
- 杜邦线若干
- 可选：按键模块或一根短接线

接线：

| ST-Link V2 | F103 板 |
|---|---|
| SWDIO | PA13 |
| SWCLK | PA14 |
| GND | GND |
| 3.3V | 3.3V |

| USB-TTL | F103 板 |
|---|---|
| TXD | PA10（F103 RX） |
| RXD | PA9（F103 TX） |
| GND | GND |
| VCC | 不接 |

注意：

- 只用一个电源。ST-Link 供 3.3 V 时，不要再从 USB-TTL 接 VCC。
- `BOOT0` 跳线保持 0，从主 Flash 启动。
- TX/RX 必须交叉。

## 2. 新建工程

1. 打开 STM32CubeIDE。
2. `File -> New -> STM32 Project`。
3. 选择 `MCU Selector`，搜索 `STM32F103C8Tx`。
4. 选中芯片，`Next`。
5. 项目名填 `sensor_terminal`，语言选 C，`Finish`。
6. 如果提示是否初始化外设，选 `Yes`。

> 不要选 `STM32F103C8Tx` 之外的型号。C8T6 通常是 64 KB Flash、20 KB SRAM，选大了可能在链接或下载时出问题。

## 3. 配置时钟和外设

在 `.ioc` 图形界面里配置：

### 3.1 时钟

1. `System Core -> RCC -> HSE` 选 `Crystal/Ceramic Resonator`。
2. 打开 `Clock Configuration`。
3. 大多数蓝色 F103 最小系统板使用 8 MHz 晶振，配置为：
   - HSE：8 MHz
   - PLLMul：x9
   - SYSCLK：72 MHz
   - AHB Prescaler：/1
   - APB1 Prescaler：/2，得到 36 MHz
   - APB2 Prescaler：/1，得到 72 MHz
   - ADC Prescaler：/6，得到 12 MHz
4. 如果 HSE 起振失败，先用 HSI 跑通，再回来查晶振和负载电容。

### 3.2 调试

`System Core -> SYS -> Debug` 选 `Serial Wire`。

### 3.3 串口

`Connectivity -> USART1`：

- Mode：`Asynchronous`
- Baud Rate：`115200`
- Word Length：`8 Bits`
- Parity：`None`
- Stop Bits：`1`
- Flow Control：`None`
- 引脚：`PA9 = USART1_TX`，`PA10 = USART1_RX`

### 3.4 GPIO

- `PC13` 设为 `GPIO_Output`，用户标签可填 `LED`。
- 如果板子有按键或你准备用短接线模拟按键：`PA1` 设为 `GPIO_Input`，Pull-up，用户标签填 `KEY`。
- 没有按键也可以先跳过，不影响本课验收。

### 3.5 代码生成

`Project Manager -> Code Generator`：

- 勾选 `Generate peripheral initialization as a pair of .c/.h files`
- 勾选 `Keep User Code when re-generating`

然后 `Project -> Generate Code`。

## 4. 让 printf 走到 USART1

### 4.1 加头文件

在 `Core/Src/main.c` 顶部的用户代码区加：

```c
#include <stdio.h>
```

### 4.2 重定向 _write

在 `main.c` 的 `/* USER CODE BEGIN 4 */` 里加：

```c
int _write(int file, char *ptr, int len)
{
    (void)file;
    HAL_UART_Transmit(&huart1, (uint8_t *)ptr, (uint16_t)len, 100u);
    return len;
}
```

### 4.3 主循环

在 `main()` 里，所有 `MX_xxx_Init()` 之后、`while (1)` 之前加：

```c
setvbuf(stdout, NULL, _IONBF, 0);
printf("\r\nboot f103 hal\r\n");

uint32_t last = 0;
```

把 `while (1)` 里的内容改成：

```c
while (1)
{
    if ((HAL_GetTick() - last) >= 500u)
    {
        last += 500u;
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        printf("tick=%lu\r\n", (unsigned long)HAL_GetTick());
    }
}
```

说明：

- PC13 上的板载 LED 通常是低电平点亮，`TogglePin` 会每 500 ms 翻转一次。
- 如果你把 PC13 的用户标签设成了 `LED`，也可以写 `HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin)`。
- `setvbuf` 让 `printf` 不缓冲，日志能立即出来。

## 5. 可选：加按键

如果 PA1 接了按键或短接线，在主循环里加：

```c
static uint32_t last_key = 0;
static uint8_t key_prev = 1;
uint8_t key_now = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_RESET) ? 0u : 1u;

if ((key_prev == 1u) && (key_now == 0u) &&
    (HAL_GetTick() - last_key > 20u))
{
    last_key = HAL_GetTick();
    printf("key pressed\r\n");
}
key_prev = key_now;
```

按下时 PA1 被拉低，松手为高。这里只做简单消抖，后面用 FreeRTOS 时再改成事件通知。

## 6. 加 HardFault 输出

打开 `Core/Src/stm32f1xx_it.c`，在文件顶部加：

```c
#include <stdio.h>
```

找到 `HardFault_Handler`，在 `while (1)` 前加：

```c
printf("HardFault\r\n");
```

测试方法：临时在 `main()` 的 `while (1)` 前加一行：

```c
*(volatile uint32_t *)0xFFFFFFF0u = 1u;
```

重新下载后，串口应看到 `HardFault`。看到后把这行删掉。

如果 HardFault 没有输出，不代表没有进入异常，可能是异常破坏了串口状态。此时用 ST-Link 调试器暂停，查看调用栈和 `CFSR/HFSR`。

## 7. 编译和下载

1. 点击 Build。
2. 点击 Debug 或 Run。
3. 第一次会要求选择调试器：选 `ST-Link`，接口选 `SWD`。
4. 下载完成后按板子复位键。

如果 ST-Link 连不上：

- 检查 GND 是否接好。
- 检查 SWDIO/SWCLK 是否接反。
- 检查 BOOT0 是否为 0。
- 检查板子是否上电，ST-Link 是否能识别目标电压。
- 先点 `Connect Under Reset` 再下载。

## 8. 看串口

用串口助手或 STM32CubeIDE 的 Terminal：

- 端口：USB-TTL 对应的 COM 口
- 波特率：115200
- 数据位：8
- 校验：None
- 停止位：1

上电或按复位后，应该看到：

```text
boot f103 hal
tick=500
tick=1000
tick=1500
...
```

## 9. 验收

- 工程能编译、能通过 ST-Link 下载。
- LED 每 500 ms 翻转。
- 串口能稳定看到 `boot` 和 `tick=`。
- 按键可选：按下能打印 `key pressed`。
- 故意写非法地址后能看到 `HardFault`，删掉测试代码后系统恢复正常。

## 10. 常见问题

### 串口没有输出

- USB-TTL 的 TX/RX 是否交叉。
- 是否共地。
- 是否选对 COM 口。
- 波特率是否 115200。
- 程序是否真的下载成功并复位。

### 串口乱码

- 时钟配置和实际晶振不一致。
- 波特率算错。
- HSE 没有起振，系统实际跑在 HSI。
- 串口助手数据位/校验/停止位不对。

### LED 不亮

- PC13 是低电平点亮，先确认代码在翻转。
- 板子上的 LED 可能不是 PC13，查原理图或丝印。
- 程序没有下载成功。

### ST-Link 连不上

- 先检查电源和 GND。
- SWDIO 接 PA13，SWCLK 接 PA14。
- BOOT0 保持 0。
- 尝试 `Connect Under Reset`。
- 检查 ST-Link 驱动和设备管理器。

### 编译后 Flash/RAM 不够

- F103C8T6 是 64 KB Flash、20 KB SRAM。
- 先不要加 FreeRTOS、OLED、浮点 printf 和大缓冲区。
- 代码优化选 `-Os`，按阶段逐步增加功能。

## 11. 这一课的核心知识点

- STM32 从上电到 main 的启动流程。
- GPIO 推挽输出、低电平点亮 LED、为什么 LED 可以这样接。
- USART1 的 TX/RX、波特率、8-N-1 帧格式。
- `printf` 重定向的本质：`_write` 最终调用 `HAL_UART_Transmit`。
- HardFault 的基本定位思路：异常栈帧、CFSR/HFSR、调试器。
- F103C8T6 的资源限制：64 KB Flash、20 KB SRAM，后续 FreeRTOS 要省着用。

