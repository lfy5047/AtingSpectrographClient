# AtingSpectrographClient 项目规则

## Global Rules

- 当创建 Git commits 时，使用 Conventional Commits 格式：`<type>[optional scope]: <description>`。
- 使用英文 Conventional Commit type，例如：`feat`、`fix`、`refactor`、`docs`、`test`、`chore`、`build`、`ci`、`perf`、`style`。
- 冒号后的 commit 描述使用中文。
- commit 描述保持简洁、祈使句风格。
- 仅在有助于澄清行为、迁移说明、Issue 引用或破坏性变更时，添加 body/footer；这些细节优先使用中文（技术术语必要时可用英文）。

## Build

- 需要编译本项目时，在仓库根目录直接运行：`.\make.bat`。

