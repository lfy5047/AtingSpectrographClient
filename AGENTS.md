# AtingSpectrographClient 项目规则

## Global Rules

- 对代码进行较大改动前，必须先检查当前 Git 分支和工作区状态。
  - 如果当前位于主分支（如 `main`、`master` 或项目约定的主开发分支），必须先创建新的任务分支，并在该分支上完成修改。
  - 如果当前已经位于非主分支，应先确认该分支是否属于当前任务；若属于当前任务，可以继续修改；若不属于当前任务，应从合适的基线创建新分支，避免混入无关改动。
  - 如果工作区存在未提交改动，必须先确认这些改动的归属，不能擅自覆盖、回滚或混入新任务。
- 当创建 Git commits 时，使用 Conventional Commits 格式：`<type>[optional scope]: <description>`。
- 使用英文 Conventional Commit type，例如：`feat`、`fix`、`refactor`、`docs`、`test`、`chore`、`build`、`ci`、`perf`、`style`。
- 冒号后的 commit 描述使用中文。
- commit 描述保持简洁、祈使句风格。
- 仅在有助于澄清行为、迁移说明、Issue 引用或破坏性变更时，添加 body/footer；这些细节优先使用中文（技术术语必要时可用英文）。

## Build

- 需要编译本项目时，在仓库根目录直接运行：`.\make.bat`。

