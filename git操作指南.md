# 本地 Git 操作指南（本项目）

> 适用对象：本仓库（FJSP-SDST 禁忌搜索求解器，`/Users/chenshiyu/Desktop/maser-0911`）。
> 当前状态：已执行 `git init`，默认分支 `main`，首次提交 `08d5cda`，工作区干净。
> 本文聚焦**本地操作**；远程关联/推送作为可选章节放在最后。

---

## 0. 本仓库约定（先读这段）

- **已纳入版本控制**：源码（`CMakeLists.txt` `main.cpp` `include/` `src/`）、评测工具（`tools/`）、算例（`instance/` 非 SDST、`instance_SDST/` 含准备时间）、成果报告（`benchmark_report*.csv` `sdst_report*.csv`）、`技术方案与改进路线.md`。
- **已忽略、不入库**（` .gitignore`）：`build/`、`cmake-build-debug/`、`output*/`、`runs/`、`.idea/`、`.workbuddy/`、`__pycache__/`、`.DS_Store`。
  - 原因：这些都是「编译产物 / 求解器运行输出 / IDE 配置 / 工具缓存」，每次构建或跑实验都会重新生成，体积大且变化频繁，没必要进仓库。
- **⚠️ 文件名注意**：源码中存在拼写错误文件 `src/Instacne.cpp`（应为 `Instance.cpp`）。本次提交未改动它，保持现状；如要改名请用 `git mv`（见 §7），不要直接改文件名否则 git 会当成"删除+新增"。

---

## 1. 查看当前状态（最常用）

```bash
git status                 # 工作区 vs 暂存区 vs 已提交 的差异概览
git status -s              # 简短模式：A=新增 M=改过 D=删除 ??=未跟踪
git log --oneline          # 提交历史（一行一条）
git log --oneline -5       # 最近 5 条
git log --stat             # 看每次提交改了哪些文件、各加减多少行
git diff                   # 工作区未暂存的改动（相对暂存区）
git diff --cached          # 已暂存、待提交 Preview（相对上次提交）
git diff HEAD              # 工作区全部改动（含已暂存）
```

> 提交前养成习惯：先 `git status` + `git diff --cached` 看一眼，确认加对了文件。

---

## 2. 提交改动（标准流程）

```bash
# 步骤 1：把改动加入暂存区
git add src/TabuSearch.cpp            # 只加某个文件
git add include/ src/                  # 加整个目录
git add -p                             # 交互式：按代码块(hunk)选择性暂存（推荐用于大改动）

# 步骤 2：提交
git commit -m "fix: 修正关键块提取中成环回退的边界条件"

# 提交信息建议写成「类型: 简述」
#   feat:   新功能/新方案          fix:    修 bug
#   refactor: 重构（行为不变）      docs:   文档/注释
#   perf:   性能/质量提升           chore:  构建/配置/忽略项
#   revert: 回退
```

**好的提交信息**：一行说清「做了什么 + 为什么」。例：
`perf: 将初始解构造次数 5→10，难例达下界数 33→41`
**差的提交信息**：`update`、`fix bug`、`改了点东西`。

---

## 3. 管理 .gitignore（忽略规则）

```bash
# 新增一条忽略规则（如想排除某日志目录）
echo "runs/logs/" >> .gitignore

# 若某文件已被 git 跟踪、但你后悔了，想改为忽略：
# 1) 先把它从 git 索引移除（不删磁盘文件）
git rm --cached output/foo.csv
# 2) 写入 .gitignore
echo "output/" >> .gitignore
# 3) 提交
git commit -m "chore: 将 output/ 移入忽略清单"
```

> 已有 `output*/` 通配，新增的 `output_xxx` 目录会自动被忽略，无需逐个加。

---

## 4. 撤销与回退（仅本地，安全操作）

| 目的 | 命令 | 说明 |
|---|---|---|
| 丢弃某个文件的**工作区**改动（未提交） | `git restore src/TabuSearch.cpp` | 或旧写法 `git checkout -- src/TabuSearch.cpp` |
| 取消**已暂存**（unstage） | `git restore --staged src/TabuSearch.cpp` | 改动还留着，只是退出暂存区 |
| 修改**最近一次**提交信息或补文件 | `git commit --amend` | 把当前暂存内容并入上一次提交（未 push 时安全） |
| 回退到某次提交（保留改动为未提交） | `git reset --soft <commit>` |  HEAD 移到目标，改动留在暂存区 |
| 回退到某次提交（改动在工作区） | `git reset --mixed <commit>` | 默认模式，改动变未暂存 |
| 回退到某次提交（彻底丢弃改动） | `git reset --hard <commit>` | **危险**：工作区改动永久丢失，仅在你百分百确定时用 |

> 原则：未推送到远程前，这些本地回退都安全；`--hard` 会删文件，务必确认。
> 若已 push 还想改历史，请用 `git revert <commit>` 生成"反向提交"，**不要 `reset --hard` 后 `push --force`**。

---

## 5. 分支基础

```bash
git branch                  # 列出本地分支（当前分支前有 *）
git branch dev             # 新建 dev 分支（不切换）
git switch dev             # 切到 dev（Git 2.23+ 推荐，旧写法 git checkout dev）
git switch -c feature-xyz  # 新建并切换（= git checkout -b feature-xyz）
git merge dev              # 把 dev 合并进当前分支
git branch -d dev          # 删除已合并的分支
```

建议：试验新方案（如 §未来方案里的"增量评估""多关键路径"）时，新建分支 `git switch -c try-incremental-eval`，跑通后再 `merge` 回 `main`，互不干扰。

---

## 6. 临时保存改动（stash）

```bash
git stash                   # 把当前未提交改动压栈暂存，工作区变干净
git stash pop               # 恢复最近一次 stash 并删除
git stash list              # 查看所有 stash
```
适合场景：正改到一半，需要切分支或先跑一轮实验，又不想 commit 半成品。

---

## 7. 重命名 / 移动已跟踪文件

```bash
git mv src/Instacne.cpp src/Instance.cpp   # 正确改名，git 记录为 rename
git commit -m "refactor: 修正源码文件名拼写 Instacne→Instance"
```
> ⚠️ 不要直接 `mv`/`ren` 改文件名——git 会当成"旧文件删除 + 新文件新增"，丢失 rename 关联与历史连续性。

---

## 8. （可选）关联远程仓库并推送

> 本仓库**当前未关联任何远程**。以下仅在你想备份到 GitHub/GitLab/公司 Git 时执行。

```bash
git branch -M main                              # 确保主分支名为 main
git remote add origin https://github.com/你/项目.git   # 替换为真实 URL
git push -u origin main                         # 首次推送并关联上游
```

**两种情况要注意：**
1. **远程仓库是空的**（新建、无提交）→ 直接 `git push -u origin main` 即可。
2. **远程仓库非空**（已有 README/初始提交）→ `push` 会被拒绝。先拉取合并：
   ```bash
   git pull --rebase origin main     # 把远程历史接在本地提交之前
   git push -u origin main
   ```

**🔴 强制推送禁令**：永远不要用 `git push --force`（或 `-f`）。它会用本地历史**覆盖**远程，导致协作者的提交丢失。确需强制时也应 `git push --force-with-lease`（更安全的变体），且只在你独自使用的分支上。

---

## 9. 本项目实用提示

- **跑实验不影响仓库**：求解输出在 `output*/`、`runs/`，已被忽略。你可以随时重跑 300k / 1M，无需 `git add`，也不会污染提交。
- **报告 CSV 要入库**：`benchmark_report*.csv` 等是成果，改了之后记得 `git add` 并提交，方便日后对照。
- **别提交大文件**：若将来某个算例或日志很大，先加进 `.gitignore`，不要直接 `git add` 进去（git 不适合存大二进制/大输出）。
- **提交粒度**：按"一个逻辑改动一次提交"，比"一天一提交"更易 review 和回退。

---

## 速查清单

```bash
git status                # 看状态
git diff --cached         # 提交前预览
git add <文件>            # 暂存
git commit -m "类型: 简述" # 提交
git log --oneline         # 看历史
git switch -c 新分支       # 试新方案时开分支
# 远程（按需）：git remote add origin <url> && git push -u origin main
```
