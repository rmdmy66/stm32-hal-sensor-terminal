# Git 与 GitHub 使用手册（本项目管理版）

## 1. 当前仓库状态

本项目已经完成：

- `git init -b main`
- 配置 `.gitignore`，忽略构建产物、CubeIDE 临时文件、Python 缓存和本地密钥
- `git add .`，当前文件已进入暂存区

还差：

- 配置提交身份
- 创建第一次提交
- 在 GitHub 创建远程仓库
- 关联 `origin` 并推送

## 2. 一次性配置提交身份

把名字和邮箱换成你的 GitHub 信息：

```powershell
git config --global user.name "你的名字"
git config --global user.email "你的邮箱"
```

如果只想对当前项目生效：

```powershell
git -C "D:\mcu+freertos项目" config user.name "你的名字"
git -C "D:\mcu+freertos项目" config user.email "你的邮箱"
```

建议使用 GitHub 的 noreply 邮箱保护隐私。GitHub 设置里开启后，格式通常是：

```text
你的GitHub用户名@users.noreply.github.com
```

检查：

```powershell
git config user.name
git config user.email
```

## 3. 第一次提交

```powershell
cd "D:\mcu+freertos项目"
git status
git commit -m "chore: initialize STM32F103C8T6 project"
git log --oneline
```

提交信息建议使用英文类型前缀：

- `feat:` 新功能
- `fix:` 修 bug
- `docs:` 文档
- `test:` 测试
- `chore:` 工程配置

例子：

```text
feat: add USART1 printf retarget
fix: recover I2C bus after sensor timeout
docs: update F103 wiring table
test: add frame parser CRC cases
```

## 4. 在 GitHub 创建仓库

1. 打开 GitHub，点右上角 `+ -> New repository`。
2. 仓库名建议：`stm32-f103-sensor-terminal`。
3. 选择 `Public` 或 `Private`。如果希望公开展示项目，选 Public。
4. **不要**勾选 `Add a README file`、`.gitignore`、`license`，因为本地已经有文件。
5. 点 `Create repository`。
6. 复制 HTTPS 地址，例如：

```text
https://github.com/你的用户名/stm32-f103-sensor-terminal.git
```

## 5. 关联远程仓库并推送

```powershell
cd "D:\mcu+freertos项目"
git remote add origin https://github.com/你的用户名/stm32-f103-sensor-terminal.git
git branch -M main
git push -u origin main
```

以后只需要：

```powershell
git push
```

## 6. 认证方式怎么选

GitHub 不接受账户密码作为 Git 密码。三选一：

### 方式 A：GitHub Desktop（最适合新手）

1. 安装 GitHub Desktop，登录 GitHub。
2. `File -> Add local repository`。
3. 选择 `D:\mcu+freertos项目`。
4. 点 `Publish repository`，填写仓库名和公开/私有。
5. 以后在界面里点 `Commit` 和 `Push` 即可。

### 方式 B：Personal Access Token（命令行）

1. GitHub 头像 -> `Settings` -> `Developer settings`。
2. `Personal access tokens` -> `Fine-grained tokens`。
3. 生成 token，给目标仓库 `Contents: Read and write` 权限。
4. 执行 `git push` 时：
   - 用户名：你的 GitHub 用户名
   - 密码：粘贴 token，不是账户密码
5. token 不要发到聊天、截图或提交到仓库。

### 方式 C：GitHub CLI

如果安装了 GitHub CLI：

```powershell
gh auth login
gh repo create stm32-f103-sensor-terminal --public --source=. --remote=origin --push
```

## 7. 每天的工作流

写代码前：

```powershell
git pull
```

写完一个可运行的小功能后：

```powershell
git status
git diff
git add .
git commit -m "feat: add USART1 DMA IDLE receiver"
git push
```

不要攒很多天的改动一次提交。每完成一个能验收的小步骤就提交一次。

## 8. 分支工作流（可选）

做一个较大的功能时：

```powershell
git switch -c feature/uart-dma
```

完成后：

```powershell
git add .
git commit -m "feat: add USART1 DMA IDLE receiver"
git push -u origin feature/uart-dma
```

然后在 GitHub 上发起 Pull Request，合并到 `main`。

## 9. 常用检查命令

```powershell
git status                 # 当前改了什么
git diff                   # 未暂存的差异
git diff --staged          # 已暂存的差异
git log --oneline --graph  # 提交历史
git remote -v              # 远程仓库地址
git branch -vv             # 本地分支和跟踪关系
git show --stat HEAD       # 最近一次提交的文件
```

## 10. 常见问题

### `src refspec main does not match any`

还没有提交。先执行 `git commit`，再推送。

### `Authentication failed`

GitHub 不再接受账户密码。改用 GitHub Desktop、PAT 或 GitHub CLI。

### `rejected non-fast-forward`

远程比本地新。先拉取：

```powershell
git pull --rebase origin main
git push
```

### `dubious ownership`

项目目录所有者和当前 Git 用户不一致。只对当前目录放行：

```powershell
git config --global --add safe.directory "D:/mcu+freertos项目"
```

### 不想提交构建产物

先检查 `.gitignore` 是否覆盖 `Debug/`、`Release/`、`.metadata/`、`.settings/`、`*.o`、`*.elf`、`*.hex`。已经误加时：

```powershell
git rm -r --cached Debug
git commit -m "chore: stop tracking build output"
```

### `LF will be replaced by CRLF`

这是 Windows 换行提示，通常不影响。项目已加入 `.gitattributes` 统一文本文件换行。

## 11. 本项目建议的提交节奏

| 阶段 | 提交信息示例 |
|---|---|
| 第 0 课 | `feat: add F103 LED and USART1 log` |
| UART DMA | `feat: add USART1 DMA IDLE receiver` |
| 环形缓冲 | `feat: add UART ring buffer` |
| ADC | `feat: add TIM triggered ADC DMA sampling` |
| I2C | `feat: add SHT30 I2C driver` |
| FreeRTOS | `feat: split acquisition and communication tasks` |
| 协议 | `feat: integrate frame parser and CRC16` |
| 上位机 | `feat: add Python serial plotter` |
| 可靠性 | `fix: recover I2C bus after timeout` |

