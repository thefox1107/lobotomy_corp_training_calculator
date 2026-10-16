# 脑叶公司员工训练计算器

用于估算员工进行异想体工作时的属性增益，以及达到训练目标所需的次数。程序为 Windows C++ 桌面应用，下载 `.exe` 后可以离线运行。

> 玩家制作的非官方辅助工具。显示的次数是规划估算值，请以游戏内实际结果为准。

## 下载与启动

1. 从 [GitHub Releases](https://github.com/thefox1107/lobotomy_corp_training_calculator/releases) 下载 `LobotomyCorpCal.exe`。如果暂时没有发布附件，可按下方说明从源码构建。
2. 将 `.exe` 放入可写入的普通文件夹，双击运行。程序会在同一目录保存 `LobotomyCorpCal.ini`，无需安装 Node.js、Java 或 Python。

## 使用方法

1. 在“员工档案”填写姓名并选择工作异想体。危险等级与对象固定绑定，自动显示。
2. 在“属性与目标”填写勇气、谨慎、自律、正义的当前具体数值、训练目标，以及已经完成的工作次数。按游戏进度选择属性上限。
3. 在“训练条件”填写培训手册和培训部加成、预计每次获得的 PE-BOX，以及工作结束时的生命和精神比例。
4. 查看右侧的五级工作偏好和训练结果。红色推荐卡显示下一项建议工作；四行结果分别显示基础偏好、单次预计增益，以及“计划 / 已做 / 剩余”次数。
5. 每完成工作后，直接修改“已完成”。员工实际属性发生变化时，也请同步修改“当前”值，程序会重新估算剩余次数。

**次数说明：**每一行对应一种工作及其训练的属性。计划次数从当前属性计算到该项目标；剩余次数为计划次数减去已完成次数，最低为 0。填写“已完成”只更新进度，不会自动改动当前属性。推荐从仍需训练且基础偏好大于 0% 的工作中选择。

**保存与迁移：**当前版本保存一份员工计划。将 `LobotomyCorpCal.exe` 和同目录的 `LobotomyCorpCal.ini` 一起备份或复制，即可保留输入与进度。

## 数据与估算范围

程序收录 84 个对象，其中 65 个具有常规四类工作的五级偏好数据；19 个工具类或无法收容的对象会显示“没有常规工作”，不参与训练推荐。名称与危险等级按本地参考工作簿核对；属性计算参考[脑叶公司 Wiki 的职员页面](https://lobotomycorp.fandom.com/zh/wiki/%E8%81%8C%E5%91%98)，偏好参考[异想体页面](https://lobotomycorp.fandom.com/zh/wiki/%E5%BC%82%E6%83%B3%E4%BD%93)。

程序会在属性跨过品质门槛后改用新等级的训练系数。它不模拟工作中断、受到伤害的过程、异想体特殊能力、饰品或 WAW 连续工作过载；预计 PE-BOX、工作结束的生命与精神比例可以根据实际结果修改。

## 从源码构建

在 Windows 上安装 MinGW-w64 `g++` 和 `windres`，然后在仓库根目录运行 `build.cmd`。生成的程序位于 `dist/LobotomyCorpCal.exe`；脚本会同时运行 C++ 计算测试。编译好的程序运行时不需要 Node.js。

异想体数据源位于 `data/`，`native/catalog.hpp` 由 `node tools/generate_catalog.mjs` 生成。程序图标在 `native/app.ico`；需要重新生成时运行 `node tools/generate_icon.mjs`。仅维护这些生成资源时需要 Node.js；数据校验可运行 `node --test tests/catalog.test.mjs`。

## 授权与来源

本项目原创程序代码、公开的数据生成脚本和图标采用 **GNU GPL v3.0 only** 授权，完整条款见 [LICENSE](LICENSE)。此选择仅适用第 3 版，不自动适用后续版本。

`data/` 中整理的异想体资料，以及由其生成的 `native/catalog.hpp` 中的资料部分，保留原始来源的权利和授权要求，不因本项目代码采用 GPL 而变为本项目原创内容。Fandom Wiki 社区内容有独立的[授权说明](https://community.fandom.com/wiki/Help%3ALicensing)；本地参考工作簿不随仓库发布。本项目不代表 Wiki 或游戏官方认可。
