# XS-VLM-OCR：大模型时代的OCR工具🚀

<div align="center">

![License](https://img.shields.io/badge/license-MIT-purple.svg)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey.svg)

![Qt](https://img.shields.io/badge/Qt-5.14.2%2B-green.svg)
![C++](https://img.shields.io/badge/C++-11%2B-blue.svg)
![AI Models](https://img.shields.io/badge/AI-Qwen%20%7C%20GLM%20%7C%20Paddle%20%7C%20Gemini%20%7C%20Doubao%20%7C%20OpenAI%E5%85%BC%E5%AE%B9-orange.svg)

***\*一场关于OCR革命的故事正在上演...\****

</div>

## 为什么我们需要一个新的OCR工具？

在数字化时代，我们每天都在与各种文字打交道：扫描文档、截图内容、手写笔记、表格数据...

传统的 OCR 工具虽然能识别文字，但往往只是"看到"而不能"理解"。

想象一下这样的场景：

> - 你需要从截图中快速提取各种类型(表格/手写体等)图片的内容
> - 你遇到一张外文图片，想要翻译的同时保持原有的格式和语境
> - 你拍了一张复杂的财务报表，不仅想提取文字，还想让AI帮你分析数据趋势
> - 你有一份手写的会议记录，希望不仅转成文字，还能自动整理成结构化的会议纪要

这就是 **XS-VLM-OCR** 诞生的原因——我们不只是在做OCR，而是在构建一个**智能视觉语言理解系统**。

## 不只是识别，更是理解

### 多模型智能引擎

- **Qwen-VL 系列**：阿里云全系列视觉语言模型，支持Qwen-VL-Plus/Max/235B-VL等
- **GLM-4V 系列**：智谱AI的多模态大模型，支持GLM-4.5V/4.6V
- **PaddleOCR-VL**：百度飞桨OCR-VL系列，支持PaddleOCR-VL
- **Gemini 系列**：谷歌系列多模态模型，支持Gemini-2.5-Flash/3.0-Lite等
- **doubao 系列**：字节豆包多模态模型，支持doubao-seed-1-8-251215等
- **通用 OpenAI 兼容**：可对接硅基流动、腾讯混元等兼容厂商
- **自定义 API**：支持接入任何与 OpenAPI 格式兼容的接口
- **Tesseract 离线**：经典 OCR 引擎，无网络依赖

### 智能提示词系统

内置丰富的提示词模板，让 AI 真正理解你的需求：

#### **识别类提示词**

```
通用识别：请识别图片中的所有文字内容，保持原有格式
表格识别：请将图片中的表格转换为Markdown格式
公式识别：请识别图片中的数学公式，输出LaTeX格式
手写识别：请识别手写文字，注意字迹模糊的部分
证件识别：请提取身份证/护照等证件的关键信息
```

#### **翻译类提示词**

```
中英互译：请将图片中的文字翻译成中文/英文
多语言翻译：请识别语言并翻译成目标语言
保持格式翻译：翻译时保持原有的排版和格式
专业术语翻译：这是技术文档，请注意专业术语的准确性
```

#### **解答类提示词**

```
题目解答：这是一道数学/物理题，请给出详细解答过程
代码解释：请解释这段代码的功能和逻辑
图表分析：请分析图表数据并总结关键趋势
文档总结：请总结文档的核心内容和要点
```

#### **整理类提示词**

```
结构化输出：请将内容整理成JSON/表格格式
会议纪要：请将会议记录整理成标准纪要格式
知识提取：请提取文档中的关键知识点
数据清洗：请规范化数据格式，去除无关信息
```

### **对话式智能交互**

> - **实时预览**：所见即所得的结果展示
> - **一键复制**：识别结果自动复制到剪贴板
> - **历史记录**：保存每次识别的图片、结果


### **极致的用户体验**

- **主题切换**：深色/浅色主题，批量导航/关闭按钮随主题适配
- **侧边栏折叠 & 可拖拽**：紧凑模式，节省屏幕空间
- **多种输入方式**：上传、粘贴、拖拽、全局快捷键截图
- **批量并发**：多图上传/拖拽，默认 4 并发，结果逐张回填，可箭头切换预览
- **异步处理**：Qt 线程池 + contextId 追踪，防卡顿
- **跨平台支持**：Win/Linux，macOS 兼容取决于 Qt 环境

## 为什么选择 XS-VLM-OCR ？

当前市场上：

| 工具                          | 截图自动识别 | 多模型切换 | 接入任意在线/离线大模型 | 本地OCR | 全平台桌面 |
| ----------------------------- | ------------ | ---------- | ----------------------- | ------- | ---------- |
| 截图工具（微信截图/Snipaste） | ✔            | ✘          | ✘                       | ✘       | ✔          |
| 传统 OCR（白描/ABBYY/uTools） | ✔            | ✘          | ✘                       | ✔(有些) | 多为插件   |
| 大模型网站（ChatGPT/Qwen）    | ✘            | ✔          | ⚠️(仅支持自家模型)       | ✘       | ⚠️(网页)    |
| PaddleOCR/Tesseract           | ✘            | ✘          | ✘                       | ✔       | 需要编译   |
| XS-VLM-OCR                    | ✔            | ✔          | ✔                       | ✔       | ✔(规划中)  |


我们要做的是：

> 截图 + 大模型 OCR + 跨平台的客户端

## 演示：看看AI如何理解你的图片

**截图自动识别**：先选择所需要的提示词，然后使用快捷键【ALT+A】截图，截图后会自动调用 AI 

![截图自动识别](https://pic1.imgdb.cn/item/693e1deafe7cfeca3824a358.gif)

### 场景一：智能识别

**通用识别**

![image-20251214000020688](https://pic1.imgdb.cn/item/693e18ccfe7cfeca3824780d.png)

**表格识别**

![image-20251214000112441](https://pic1.imgdb.cn/item/693e18ebfe7cfeca38247877.png)

**手写体识别**

![image-20251214000711911](https://pic1.imgdb.cn/item/693e18f8fe7cfeca382478a6.png)

### 场景二：文档翻译

![image-20251214093346138](https://pic1.imgdb.cn/item/693e1902fe7cfeca382478c4.png)

### 场景三：解答问题

![image-20251214093653449](https://pic1.imgdb.cn/item/693e190efe7cfeca382478f9.png)

### 场景四：整理内容

![image-20251213184152193](https://pic1.imgdb.cn/item/693e191afe7cfeca38247926.png)

## 快速上手：3分钟开启AI识别之旅

### 第一步：下载安装

法1：直接下载编译好的版本

- 夸克云：[https://pan.quark.cn/s/cfbe6c3b0bfc](https://pan.quark.cn/s/cfbe6c3b0bfc)
- 访问 [GitHub Releases](https://github.com/xstongxue/XS-VLM-OCR/releases) 页面下载

法2：从源码编译

```
git clone https://github.com/xstongxue/XS-VLM-OCR.git
cd XS-VLM-OCR
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH="你的Qt路径"
cmake --build . --config Release
```

### 第二步：获取API密钥

> 完整指南：[XS-VLM-OCR-模型配置完整教程](https://xiaoshuai.site/xiaoshuai/note_1765682050905_28209)

1. 访问各个大模型官方平台
2. 注册账号并创建 API KEY
3. 新用户通常有免费额度和免费模型使用，足够体验所有功能

### 第三步：配置模型

1. 首次运行会自动读取配置文件（可以直接在配置文件 `model_config.json` 内填入API KEY）
2. 点击【设置】按钮
3. 在"模型配置"标签页填入API KEY，模型列表里可以自定义模型
4. 配置完成点击【保存】按钮，然后重启软件即可

![image-20251214095026345](https://pic1.imgdb.cn/item/693e2cb525ec9c13612c7e17.png)

### 第四步：开始使用

1. 上传图片或粘贴截图（可选）
2. 选择合适的提示词模板
3. 点击"询问AI"开始识别
4. 结果自动复制到剪贴板

## 技术架构：为开发者而生

[加入我们：参与教程-技术架构：为开发者而生.md](https://github.com/xstongxue/XS-VLM-OCR/blob/main/docs/%E5%8F%82%E4%B8%8E%E6%95%99%E7%A8%8B-%E6%8A%80%E6%9C%AF%E6%9E%B6%E6%9E%84%EF%BC%9A%E4%B8%BA%E5%BC%80%E5%8F%91%E8%80%85%E8%80%8C%E7%94%9F.md)

## 功能规划

### 已完成功能
v1.0.0：2025.12.10

- [√] 全局快捷键截图（ALT+A）

- [√] 主流大模型支持（Qwen、GLM、Paddle、Custom、Tesseract）

- [√] 智能提示词管理（识别、翻译、解答、整理四大类）

- [√] 现代化UI界面（主题切换、侧边栏折叠）

- [√] 异步任务处理（多线程并发）

v1.1.0：2025.12.31

- [√] 支持新的模型：谷歌、混元、豆包、硅基流动、离线部署的Ollama系列

- [√] 支持新的功能：批量图片异步并发处理、多屏截图、侧边栏可拖拽、结果Markdown预览
- [√] 异步任务处理（线程池，contextId 追踪）

- [√] 跨平台支持（Win && Linux）

v1.2.0：2025.02.06

- [√] 历史记录管理（SQLite持久化，筛选、搜索、分页）

- [√] 缓存去重功能（防止重复处理相同内容，提高速度）

- [√] 结果导出多格式（Markdown、PDF、Word、Excel）

### 规划/在做

- [ ] 支持Docx / PDF文件上传
- [ ] 更多平台支持：Mac / Android


## 加入我们🤝

### 如何贡献

1. **报告问题**：发现bug？在GitHub提交Issue
2. **功能建议**：有好想法？我们很乐意听取
3. **代码贡献**：Fork项目，提交Pull Request
4. **文档完善**：帮助改进文档和教程
5. **翻译工作**：支持更多语言界面


## 联系我们

- **微信公众号**：小帅随笔
- **项目主页**：[https://github.com/xstongxue/XS-VLM-OCR](https://github.com/xstongxue/XS-VLM-OCR)
- **问题反馈**：[https://github.com/xstongxue/XS-VLM-OCR/issues](https://github.com/xstongxue/XS-VLM-OCR/issues)
- **讨论交流**：[https://github.com/xstongxue/XS-VLM-OCR/discussions](https://github.com/xstongxue/XS-VLM-OCR/discussions)


## 开源协议

本项目采用 MIT 许可证：

- ✅ 商业使用
- ✅ 修改源码
- ✅ 分发软件
- ✅ 私人使用
- ⚠️ 需保留版权声明


## 致谢

感谢以下开源项目：

- **[PaddleOCR](https://github.com/PaddlePaddle/PaddleOCR)**
- **[Tesseract OCR](https://github.com/tesseract-ocr/tesseract)**
- **[Qt Framework](https://www.qt.io/)**



<div align="center">


## 🌟如果这个项目对你有帮助，请给我们一个Star！

Made with ❤️ by XS-VLM-OCR Team

*"不只是看见，更要理解"*

</div>



[![Star History Chart](https://api.star-history.com/svg?repos=xstongxue/XS-VLM-OCR&type=date&legend=top-left)](https://www.star-history.com/#xstongxue/XS-VLM-OCR&type=date&legend=top-left)