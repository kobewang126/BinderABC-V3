# V3 离线阅读器

在本目录执行 `npm ci`，然后执行 `npm run build`。生成的 `dist/BinderABC-V3/` 可直接打开 `index.html`，也可作为静态网站发布。阅读包包含当前 0～7 篇及各篇自测，源码来自本版 `examples/`；构建会检查章节源码片段、图示事件和单步源码锚点。概念图的步骤是结构示意，并非执行轨迹。

终端提供各篇主要实验的有限命令模拟回放，不会执行程序或加载驱动。提供三个共享模拟状态的终端，支持第 7.4 章的 ServiceManager、服务端和客户端实验。正常实验应在 Linux 环境中按章节说明运行。

构建还需要 Python 3（使用标准库生成 ZIP，无需安装 Python 包）。每次构建会重新生成阅读目录，清除旧产物，并自动生成：

- `dist/BinderABC-V3/BinderABC-V3-source.zip`：页面顶部“下载源码 ZIP”按钮的下载文件，包含全部章节源码、测试及使用说明，不含 Git 历史和编译产物。
- `dist/BinderABC-V3.zip`：完整离线阅读包，包含上述源码 ZIP。
- `dist/BinderABC-V3-deploy.zip`：静态部署包，解压后根目录即为 `index.html`。

执行 `npm run verify` 检查源码跳转、单步演示、源码下载与模拟终端。

部署时上传整个 `dist/BinderABC-V3/` 目录，保留源码 ZIP；相对链接同时适用于静态托管和离线阅读。
