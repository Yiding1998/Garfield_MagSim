# MicroscopicSpaceCharge 说明文档

本文档说明 `MicroscopicSpaceCharge.C` 的功能、物理模型、模拟流程、统计量定义、ROOT 输出内容和运行方法。代码基于 Garfield++ 和 ROOT，用于研究微米级气隙中单个初始电子触发的电子雪崩，并可比较不同气压、电压、磁场和空间电荷开关条件下的雪崩增益、吸附、末态电子分布和扩散随时间的变化。

## 1. 程序目标

本程序模拟一个平行板气体探测器气隙中的电子雪崩过程。默认几何为 215 um 气隙，上极板加负高压，下极板接地，初始电子从靠近上极板的位置出发，在电场中向下极板漂移并发生碰撞、激发、电离、吸附等过程。

程序主要回答以下问题：

1. 单个初始电子在给定气隙、电压、气压和磁场下能产生多少雪崩电子，即雪崩增益是多少。
2. 雪崩过程中总电离电子数、被吸附电子数、收集到下极板的电子数是多少。
3. 多个事例中，每次雪崩的增益是否有涨落。
4. 电子云在雪崩发展过程中的空间扩散如何随时间变化。
5. 雪崩结束后，收集电子的末态位置和到达时间分布是什么样的。
6. 末态位置分布能否用高斯拟合，并由拟合得到横向扩散宽度。
7. 开启或关闭空间电荷效应后，上述结果如何变化。
8. 改变气压和磁场后，增益、吸附和扩散等参数如何变化。

## 2. 依赖环境

代码依赖：

- Garfield++
- ROOT
- CMake
- 支持 C++17 或更新标准的编译器，具体取决于本地 Garfield++ 配置

`CMakeLists.txt` 中通过：

```cmake
find_package(Garfield REQUIRED)
target_link_libraries(MicroscopicSpaceCharge Garfield::Garfield)
```

链接 Garfield++，因此运行前需要保证 Garfield++ 的安装路径能被 CMake 找到。

## 3. 编译方法

在项目根目录执行：

```bash
cmake -S . -B build
cmake --build build
```

编译后可执行文件位于：

```bash
./build/MicroscopicSpaceCharge
```

## 4. 运行命令

程序支持命令行参数：

```bash
./build/MicroscopicSpaceCharge [nEvents] [gapUm] [pressureAtm] [voltageAbs] [magFieldY] [enableSpaceCharge]
```

各参数含义如下：

| 参数 | 含义 | 单位 | 默认值 |
| --- | --- | --- | --- |
| `nEvents` | 雪崩事例数 | 无量纲 | `3` |
| `gapUm` | 气隙厚度 | um | `215` |
| `pressureAtm` | 气体压强 | atm | `1.0` |
| `voltageAbs` | 电势差绝对值，代码内部转换为负高压 | V | `750` |
| `magFieldY` | y 方向磁场 | T | `0.0` |
| `enableSpaceCharge` | 是否开启空间电荷效应，`1` 开启，`0` 关闭 | 无量纲 | `1` |

示例：

```bash
./build/MicroscopicSpaceCharge 3 215 1 600 0.5 1

./build/MicroscopicSpaceCharge 3 590 0.1 1600 0

./MicroscopicSpaceCharge 3 590 0.1 1600 0
```

表示：

- 模拟 3 次独立雪崩；
- 气隙厚度为 215 um；
- 压强为 1 atm；
- 电势差为 600 V，代码内部设置上极板电势为 `-600 V`；
- 施加 y 方向磁场 `0.5 T`；
- 开启空间电荷效应。

如果关闭空间电荷效应：

```bash
./build/MicroscopicSpaceCharge 3 215 1 600 0.5 0
```

## 5. 输出文件命名

ROOT 输出文件保存在程序运行目录下，命名格式为：

```cpp
TString::Format("%dum_%.2fatm_%dV_%.2fT_%de_SC%d.root",
                gapUm, pressureAtm, abs(voltage), magFieldY,
                nEvents, enableSpaceCharge ? 1 : 0)
```

例如：

```text
215um_1.00atm_600V_0.50T_3e_SC1.root
```

文件名包含：

- 气隙厚度；
- 压强；
- 电压；
- 磁场；
- 事例数；
- 空间电荷开关状态。

这样便于批量扫描不同压强和磁场时区分输出。

## 6. 几何、电场和初始条件

### 6.1 平行板几何

代码使用 `ComponentAnalyticField` 构造一维平行板电场：

```cpp
pp.AddPlaneY(posBottomPlane, 0.);
pp.AddPlaneY(posTopPlane, voltage);
```

其中：

```cpp
posBottomPlane = 0.;
posTopPlane = gapUm * 1.e-4;
```

Garfield++ 中长度单位为 cm，因此：

```text
1 um = 1e-4 cm
```

当 `gapUm = 215` 时：

```text
gap = 215 um = 0.0215 cm
```

下极板位置为 `y = 0`，上极板位置为 `y = gap`。

### 6.2 电压和电场方向

命令行传入的是电势差绝对值，代码内部使用：

```cpp
voltage = -abs(inputVoltage)
```

因此上极板电势为负值，下极板为 0 V。电子带负电，会在电场力作用下从靠近上极板的位置向下极板漂移。

如果忽略空间电荷，平行板电场大小近似为：

```text
E = |V| / gap
```

例如 `gap = 215 um`、`V = 600 V` 时：

```text
E = 600 V / 0.0215 cm = 27907 V/cm
```

### 6.3 初始电子

每个事例从一个初始电子开始：

```cpp
x0 = 0.;
y0 = posTopPlane - 0.1e-4;
z0 = 0.;
t0 = 0.;
e0 = 0.1;
```

即初始电子位于上极板下方 0.1 um 处，初始能量为 0.1 eV。

## 7. 气体模型

气体使用 Garfield++ 的 `MediumMagboltz`：

```cpp
gas.SetComposition("ar", 93, "co2", 7);
gas.SetTemperature(293.15);
gas.SetPressure(760. * pressureAtm);
gas.LoadIonMobility("IonMobility_Ar+_Ar.txt");
gas.Initialise(true);
gas.EnablePenningTransfer();
```

当前源码中的气体组分为：

```text
C2H2F4/iC4H10/SF6 = 60/30/10
```

温度为：

```text
293.15 K
```

压强由命令行参数 `pressureAtm` 控制，代码转换为 Torr：

```text
pressureTorr = 760 * pressureAtm
```

`gas.Initialise(true)` 会生成或读取 Magboltz 碰撞表。微观雪崩追踪过程中，电子的碰撞、电离、吸附、散射等由 Magboltz 数据控制。

`EnablePenningTransfer()` 请求开启 Penning 转移。当前 C2H2F4/iC4H10/SF6 混合气体在所用 Garfield++/Magboltz 数据中没有实现默认 Penning 参数，运行时会给出提示，因此当前配置通常不会产生 Penning 电子。若更换为支持 Penning 参数的气体，新增的来源统计会将其单独标记。

代码当前仍加载 `IonMobility_Ar+_Ar.txt`。这不是 C2H2F4/iC4H10/SF6 混合气体的专用离子迁移率数据；涉及离子漂移时间或空间电荷长期演化的定量结论时，应换成与实际离子种类和混合气体相匹配的数据。

## 8. 磁场设置

磁场施加在 y 方向：

```cpp
pp.SetMagneticField(0., magFieldY, 0.);
rings.SetMagneticField(0., magFieldY, 0.);
aval->EnableMagneticField(magFieldY != 0.);
```

也就是说当前磁场方向为：

```text
B = (0, By, 0)
```

由于平行板电场也主要沿 y 方向，当前设置相当于研究 `E` 与 `B` 平行或反平行时的情形。若希望研究横向磁场对电子漂移和扩散的影响，应将磁场改为 x 或 z 方向，例如 `B = (Bx, 0, 0)` 或 `B = (0, 0, Bz)`。

## 9. 主要物理过程

### 9.1 微观电子雪崩

电子雪崩由 `AvalancheMicroscopic` 模拟：

```cpp
aval = std::make_unique<AvalancheMicroscopic>();
aval->SetSensor(&sensor);
aval->AddElectron(x0, y0, z0, t0, e0);
aval->ResumeAvalanche();
```

`AvalancheMicroscopic` 会逐个追踪电子，并根据 Magboltz 碰撞表随机决定电子与气体分子的碰撞类型。关键过程包括：

- 弹性碰撞；
- 非弹性激发；
- 电离，产生新的电子和正离子；
- 吸附，电子被气体分子吸附并形成负离子；
- 电子离开漂移区域或撞击极板；
- 在电场、磁场中漂移和扩散。

每当发生电离时，代码通过用户回调统计总电离数，并在下一时间片开始时加入正离子：

```cpp
void userHandleIonisation(...) {
  ++totalIonisationElectrons;
  drift->AddIon(x, y, z, tmin + timestep);
}
```

每当发生吸附时，代码统计总吸附数，并加入负离子：

```cpp
void userHandleAttachment(...) {
  ++totalAttachedElectrons;
  drift->AddNegativeIon(x, y, z, tmin + timestep);
}
```

### 9.2 正离子和负离子漂移

离子漂移由 `AvalancheMC` 模拟：

```cpp
drift = std::make_unique<AvalancheMC>();
drift->SetSensor(&sensor);
drift->ResumeAvalanche();
```

正离子来自电子电离。负离子来自电子吸附。它们漂移速度远低于电子，但会作为空间电荷源影响后续电子运动。

### 9.3 空间电荷效应

空间电荷通过 `ComponentChargedRing` 近似实现。程序将当前时间片中的电子、正离子、负离子作为空间电荷源加入到传感器中：

- 电子电荷为 `-1`；
- 正离子电荷为 `+1`；
- 负离子电荷为 `-1`。

代码当前不是把每一个粒子都单独加入为 charged ring，而是沿 y 方向分 bin 后进行合并：

```cpp
buildBinnedSpaceCharge(posBottomPlane, posTopPlane, spaceChargeBinWidth);
```

每个 bin 中的同类粒子会合并为一个等效带电环：

```cpp
rings.AddChargedRing(meanX, meanY, meanZ, totalCharge);
```

这样可以显著减少空间电荷源数量，从而降低 `ComponentChargedRing` 计算开销。

当前 bin 宽度固定为：

```cpp
spaceChargeBinWidth = 2.e-4 cm = 2 um
```

bin 数量根据气隙厚度自适应：

```cpp
nBins = ceil(gap / spaceChargeBinWidth)
```

例如 215 um 气隙会得到约 108 个 bin。

需要注意：`ComponentChargedRing` 是轴对称带电环近似，不能完全等价于真实三维空间电荷分布。对于研究趋势和降低计算成本是有用的，但若要做严格三维空间电荷场分析，需要更精细的场求解方法。

## 10. 时间推进算法

程序不是一次性追踪完整雪崩，而是按时间窗口分帧推进。

每个事例的主循环结构为：

1. 清空上一帧空间电荷源。
2. 统计当前仍在运动的电子、正离子、负离子数量和平均位置。
3. 若开启空间电荷，则按 y 方向分 bin，重建当前帧的空间电荷 charged rings。
4. 输出当前帧粒子数。
5. 根据粒子增长情况自适应调整时间步长。
6. 更新 charged-ring 组件中心位置。
7. 检查是否达到硬电离数停止条件。
8. 检查是否已经没有活电子。
9. 设置 `AvalancheMicroscopic` 时间窗口并继续追踪电子。
10. 收集已经停止运动的电子末态。
11. 若存在离子，则推进离子漂移。
12. 记录当前时刻电子扩散。
13. 进入下一帧。

### 10.1 时间窗口

电子雪崩每次只追踪：

```cpp
[tmin, tmin + timestep]
```

然后调用：

```cpp
aval->SetTimeWindow(tmin, tmin + timestep);
aval->ResumeAvalanche();
```

离子漂移也使用相同时间窗口：

```cpp
drift->SetTimeWindow(tmin, tmin + timestep);
drift->ResumeAvalanche();
```

### 10.2 自适应时间步长

程序开启了自适应时间步长：

```cpp
enableAdaptiveTimestep = true;
```

默认初始时间步长为：

```cpp
timestep = 0.0025 ns
```

随后通常调整到 `0.05 ns`，并根据新产生离子数相对于当前电子数的比例进行调节：

```cpp
ratio = newIons / currentElectronCount
```

若新增离子很少且电子数少于 100，则使用较大的 `0.05 ns`。否则根据 `ratio` 增减时间步长，以避免雪崩增长过快时单步跨度过大。

这个算法是经验性的，主要用于在计算速度和时间分辨率之间折中。

## 11. 雪崩增益和粒子统计

### 11.1 增益定义

代码中每个事例的雪崩增益定义为：

```cpp
gain = 1 + totalIonisationElectrons
```

其中 `1` 是初始电子，`totalIonisationElectrons` 是该事例中累计发生的电离次数。每次电离产生一个新电子，因此该定义对应总产生电子数。

注意：这个增益不等于最终被下极板收集的电子数。因为部分电子可能被吸附，部分电子也可能在硬停止时仍未运动结束。

### 11.2 总电离电子数

`totalIonisationElectrons` 在电离回调中累加：

```cpp
++totalIonisationElectrons;
```

输出到：

- `avalanche_events.ionisations`
- `run_summary.totalIonisations`

### 11.3 被吸附电子数

`totalAttachedElectrons` 在吸附回调中累加：

```cpp
++totalAttachedElectrons;
```

输出到：

- `avalanche_events.attachments`
- `run_summary.totalAttachments`

### 11.4 收集电子数

代码检查电子末态是否到达下极板附近：

```cpp
abs(endpoint.y - posBottomPlane) <= collectionTolerance
```

其中：

```cpp
collectionTolerance = 5.e-5 cm = 0.5 um
```

若电子状态为离开漂移介质或打到平面，并且末态位置接近下极板，则计入 collected electrons。

输出到：

- `avalanche_events.collectedElectrons`
- `run_summary.totalCollectedElectrons`

### 11.5 硬停止标记

程序设置了两个与雪崩规模相关的限制：

```cpp
avalancheSizeLimit = 5000;
maxTotalIonisations = 50000;
```

`avalancheSizeLimit` 是 Garfield++ 内部雪崩规模限制，它不是严格的总增益上限。由于雪崩分时间窗口继续追踪，且 Garfield++ 的限制作用在内部追踪过程上，因此最终统计到的总增益可能超过这个值。

真正的硬停止条件是：

```cpp
if (totalIonisationElectrons >= maxTotalIonisations) {
  stoppedByIonisationLimit = 1;
  break;
}
```

若某个事例因为达到该条件而提前停止，则：

```text
avalanche_events.stoppedByIonisationLimit = 1
```

若为正常结束，则为 `0`。

分析数据时，如果 `stoppedByIonisationLimit = 1`，说明该事例被人为截断。此时末态电子分布、收集电子数和扩散终值都应谨慎使用。

## 12. 电子扩散统计

每一帧结束后，代码统计当前仍活跃或处于时间窗口外的电子末端位置。对这些电子计算：

```text
meanX, meanY, meanZ
sigmaX, sigmaY, sigmaZ
sigmaT
```

其中：

```text
sigmaX = sqrt(<x^2> - <x>^2)
sigmaY = sqrt(<y^2> - <y>^2)
sigmaZ = sqrt(<z^2> - <z>^2)
```

横向扩散宽度定义为：

```text
sigmaT = sqrt(0.5 * (sigmaX^2 + sigmaZ^2))
```

该定义假设横向方向为 x 和 z，漂移方向为 y。

扩散随时间的变化写入：

```text
electron_diffusion
```

并绘制成：

```text
diffusion
diffusion_png
```

其中每个事例一条曲线，横轴为时间 `time [ns]`，纵轴为 `sigmaT [cm]`。

## 13. 末态电子位置分布和高斯拟合

程序会记录已经停止运动的电子末态：

```text
electron_endpoints
```

包括：

- 事例编号；
- Garfield 状态码；
- 权重；
- x、y、z 位置；
- 停止时间。

用于高斯拟合的对象是收集到下极板的电子，即 collected endpoints。程序对它们分别建立直方图：

- `endpoint_x`
- `endpoint_y`
- `endpoint_z`
- `endpoint_time`

并分别进行高斯拟合：

```text
fit_endpoint_x
fit_endpoint_y
fit_endpoint_z
fit_endpoint_time
```

### 13.1 拟合区间

拟合区间采用直方图均值和 RMS 的正负三倍范围：

```text
[mean - 3 * RMS, mean + 3 * RMS]
```

同时限制在直方图自身范围内。

若有效条目数少于 10，或 RMS 小于等于 0，则不进行拟合，fit status 记为 `-1`。

### 13.2 拟合结果

拟合结果写入：

```text
endpoint_fit_summary
```

包括：

- `statusX`, `statusY`, `statusZ`, `statusTime`
- `amplitudeX`, `amplitudeY`, `amplitudeZ`, `amplitudeTime`
- `meanX`, `meanY`, `meanZ`, `meanTime`
- `sigmaX`, `sigmaY`, `sigmaZ`, `sigmaTime`
- `sigmaTransverse`
- `fitMinX`, `fitMaxX`
- `fitMinY`, `fitMaxY`
- `fitMinZ`, `fitMaxZ`
- `fitMinTime`, `fitMaxTime`

其中横向末态扩散宽度定义为：

```text
sigmaTransverse = sqrt(0.5 * (sigmaX^2 + sigmaZ^2))
```

只有当 x 和 z 两个方向的拟合都成功时，`sigmaTransverse` 才有效；否则为 `-1`。

### 13.3 拟合图

末态分布和拟合曲线保存在：

```text
endpoint_distributions
endpoint_distributions_png
```

图中每个 pad 的左上方显示：

- fit status；
- 拟合区间；
- 幅度 `A`；
- 均值 `mu`；
- 宽度 `sigma`。

这些参数也可以直接从 `endpoint_fit_summary` 中读取，建议正式分析时优先使用 TTree 中的数据，而不是从图上读数。

## 14. ROOT 文件内容

输出 ROOT 文件中主要对象如下。

### 14.1 `run_summary`

单行 TTree，记录一次运行的总体配置和总统计量：

| 分支 | 含义 |
| --- | --- |
| `numberOfEvents` | 事例数 |
| `avalancheSizeLimit` | Garfield++ 内部雪崩规模限制 |
| `maxTotalIonisations` | 代码层面的硬电离数停止阈值 |
| `spaceChargeBinWidth` | 空间电荷 y-bin 宽度，单位 cm |
| `spaceChargeNBins` | 当前气隙下的空间电荷 bin 数 |
| `totalIonisations` | 所有事例总电离次数 |
| `totalAttachments` | 所有事例总吸附次数 |
| `totalCollectedElectrons` | 所有事例总收集电子数 |
| `gapThickness` | 气隙厚度，单位 cm |
| `gapUm` | 气隙厚度，单位 um |
| `pressureAtm` | 压强，单位 atm |
| `pressureTorr` | 压强，单位 Torr |
| `voltage` | 上极板电压，单位 V，通常为负值 |
| `magFieldY` | y 方向磁场，单位 T |
| `enableSpaceCharge` | 空间电荷开关 |
| `initialX`, `initialY`, `initialZ` | 初始电子位置，单位 cm |
| `initialEnergy` | 初始电子能量，单位 eV |

### 14.2 `avalanche_events`

每个事例一行：

| 分支 | 含义 |
| --- | --- |
| `event` | 事例编号 |
| `gain` | 雪崩增益，定义为 `1 + ionisations` |
| `avalancheIons` | 电离产生的正离子数，当前等于 `ionisations` |
| `ionisations` | 该事例总电离次数 |
| `attachments` | 该事例总吸附次数 |
| `collectedElectrons` | 该事例收集到下极板的电子数 |
| `stoppedByIonisationLimit` | 是否因硬电离数限制提前停止 |

### 14.3 `electron_endpoints`

记录电子停止运动时的位置和状态：

| 分支 | 含义 |
| --- | --- |
| `event` | 事例编号 |
| `status` | Garfield 电子终止状态码 |
| `weight` | 电子权重 |
| `x`, `y`, `z` | 末态坐标，单位 cm |
| `time` | 末态时间，单位 ns |

### 14.4 `endpoint_fit_summary`

记录 collected endpoints 的高斯拟合结果，详见第 13 节。

### 14.5 `electron_diffusion`

记录雪崩过程中逐帧扩散：

| 分支 | 含义 |
| --- | --- |
| `event` | 事例编号 |
| `frame` | 帧编号 |
| `time` | 当前时间，单位 ns |
| `nElectrons` | 用于扩散统计的电子数 |
| `meanX`, `meanY`, `meanZ` | 当前电子云平均位置，单位 cm |
| `sigmaX`, `sigmaY`, `sigmaZ` | 三个方向的位置标准差，单位 cm |
| `sigmaT` | 横向扩散宽度，单位 cm |

### 14.6 图像和直方图对象

程序在 batch 模式下运行，不在屏幕显示图像。所有图像都写入 ROOT 文件：

| 对象 | 含义 |
| --- | --- |
| `endpoint_x`, `endpoint_y`, `endpoint_z`, `endpoint_time` | 收集电子末态分布直方图 |
| `fit_endpoint_x`, `fit_endpoint_y`, `fit_endpoint_z`, `fit_endpoint_time` | 对应高斯拟合函数 |
| `endpoint_distributions` | 末态分布和拟合曲线 canvas |
| `endpoint_distributions_png` | 末态分布 canvas 的 PNG 图像对象 |
| `diffusion` | 扩散随时间变化 canvas |
| `diffusion_png` | 扩散 canvas 的 PNG 图像对象 |
| `diffusion_graph` | 多事例扩散曲线 |
| `diffusion_event_graphs/` | 每个事例的 `diffusion_sigma_t_event_N` 曲线子目录，避免大量事例占满 ROOT 顶层目录 |
| `field` | 电场分布 canvas |
| `field_png` | 电场图 PNG 图像对象 |

若 `plotDrift = true`，还会输出漂移轨迹相关图像。目前代码中 `plotDrift = false`，因此默认不画漂移轨迹。

## 15. 如何读取 ROOT 结果

### 15.1 查看运行配置

```bash
root -l 215um_1.00atm_600V_0.50T_3e_SC1.root
```

进入 ROOT 后：

```cpp
run_summary->Scan();
```

### 15.2 查看每个事例增益

```cpp
avalanche_events->Scan("event:gain:ionisations:attachments:collectedElectrons:stoppedByIonisationLimit");
```

### 15.3 查看扩散随时间变化

```cpp
electron_diffusion->Scan("event:frame:time:nElectrons:sigmaT");
```

### 15.4 查看拟合结果

```cpp
endpoint_fit_summary->Scan();
```

### 15.5 打开 ROOT 中保存的图像

在 ROOT 中可以查看 canvas：

```cpp
endpoint_distributions->Draw();
diffusion->Draw();
field->Draw();
```

PNG 图像对象也保存在 ROOT 文件里，例如：

```cpp
endpoint_distributions_png->WriteImage("endpoint_distributions.png");
diffusion_png->WriteImage("diffusion.png");
field_png->WriteImage("field.png");
```

## 16. 典型参数扫描方法

若要研究不同压强和磁场下增益、扩散的变化，可以用 shell 循环批量运行。例如：

```bash
for p in 0.5 1.0 1.5; do
  for b in 0.0 0.5 1.0; do
    ./build/MicroscopicSpaceCharge 3 215 "$p" 600 "$b" 1
  done
done
```

每组参数会生成不同文件名，便于后续比较。

分析时建议重点比较：

- `avalanche_events.gain`
- `avalanche_events.attachments`
- `avalanche_events.collectedElectrons`
- `avalanche_events.stoppedByIonisationLimit`
- `endpoint_fit_summary.sigmaTransverse`
- `electron_diffusion.sigmaT` 随时间变化

若某组参数中大量事例 `stoppedByIonisationLimit = 1`，说明当前硬停止阈值不足以完整追踪雪崩终态。此时应该提高 `maxTotalIonisations`，或降低电压、气隙、事例数，或关闭空间电荷做对照。

## 17. 物理严谨性和适用范围

### 17.1 适合研究的内容

当前代码适合用于：

- 比较不同电压、气压、磁场下雪崩增益的趋势；
- 比较开启和关闭空间电荷效应后的相对变化；
- 观察电子扩散随时间的发展；
- 估计收集电子云末态横向扩散宽度；
- 研究吸附对有效收集电子数的影响；
- 做参数扫描和 ROOT 后处理。

### 17.2 需要谨慎解释的内容

以下结果需要谨慎解释：

1. 空间电荷由 charged-ring 近似描述，不能完全代表真实三维电荷分布。
2. 当前磁场沿 y 方向，与主漂移方向相同；若目标是横向磁场效应，需要改变磁场方向。
3. `gain = 1 + ionisations` 是总产生电子数，不是收集电子数。
4. `avalancheSizeLimit` 不是最终增益上限；真正的硬停止由 `maxTotalIonisations` 控制。
5. 若硬停止触发，末态分布和扩散终值不是完整雪崩终态。
6. 高斯拟合只是一种有效宽度估计。若末态分布明显非高斯，应结合直方图形状和 RMS 一起判断。
7. 代码默认只记录 collected endpoints 的高斯拟合结果，没有对每个事例分别拟合末态分布；当前拟合是所有事例合并后的分布。

## 18. 代码结构说明

主要函数和结构如下。

### 18.1 `writeCanvasWithPngImage`

功能：

- 更新 `TCanvas`；
- 将 canvas 写入 ROOT 文件；
- 将 canvas 转换为 `TImage`；
- 将 PNG 图像对象写入 ROOT 文件。

这样图像不会显示在屏幕上，而是保存在 ROOT 文件内部。

### 18.2 `ElectronSpread`

保存某一帧电子云扩散统计量：

- 电子数；
- 平均位置；
- x、y、z 三方向标准差；
- 横向标准差 `sigmaT`。

### 18.3 `get_electron_spread`

遍历当前电子，筛选仍活跃或处于时间窗口外的电子，读取其路径末端位置，计算电子云扩散。

函数中包含空路径保护：

```cpp
if (electron.path.empty()) continue;
```

避免访问空 path 的 `.back()`。

### 18.4 `userHandleIonisation`

电离回调函数。每发生一次电离：

- 总电离数加一；
- 在下一时间片起点加入一个正离子。

### 18.5 `userHandleAttachment`

吸附回调函数。每发生一次吸附：

- 总吸附数加一；
- 在下一时间片起点加入一个负离子。

### 18.6 `get_mean`

统计当前电子、正离子、负离子的数量和平均位置，用于输出当前帧粒子数，并辅助更新空间电荷组件中心。

### 18.7 `SpaceChargeBin`

保存某个 y-bin 内同类带电粒子的合并信息：

- 粒子数；
- 总电荷；
- x、y、z 坐标和。

### 18.8 `buildBinnedSpaceCharge`

沿 y 方向自适应分 bin，将电子、正离子、负离子分别合并，再加入 `ComponentChargedRing`。

这是当前代码中降低空间电荷计算量的核心优化。

### 18.9 `main`

完成以下任务：

1. 解析命令行参数；
2. 设置 ROOT batch 模式；
3. 建立气体、电场、磁场和传感器；
4. 循环模拟多个雪崩事例；
5. 统计增益、电离、吸附、收集电子和扩散；
6. 写入 ROOT TTrees、直方图、拟合函数、canvas 和 PNG 图像对象。

## 19. 常见问题

### 19.1 为什么设置了雪崩规模限制，增益仍可能超过限制？

`AvalancheMicroscopic::EnableAvalancheSizeLimit` 是 Garfield++ 内部用于控制一次微观雪崩追踪规模的保护，不是代码最终统计的总增益硬上限。由于程序按时间窗口多次调用 `ResumeAvalanche()`，累计电离数可能超过该值。

若要限制整个事例的总雪崩规模，应使用代码中的：

```cpp
maxTotalIonisations
```

并查看：

```text
stoppedByIonisationLimit
```

判断事例是否被截断。

### 19.2 为什么 collected electrons 小于 gain？

因为：

- 部分电子被吸附；
- 部分电子可能未到达下极板；
- 若硬停止触发，部分电子仍未完成漂移；
- `gain` 表示总产生电子数，而不是最终收集电子数。

### 19.3 为什么扩散统计和末态拟合得到的 sigma 不完全一样？

两者统计对象不同：

- `electron_diffusion.sigmaT` 是雪崩过程中每一帧仍在运动或处于时间窗口外的电子云宽度；
- `endpoint_fit_summary.sigmaTransverse` 是最终被下极板收集的电子末态分布高斯拟合宽度。

前者描述动态过程，后者描述收集终态。

### 19.4 为什么空间电荷计算会很慢？

空间电荷会改变每个粒子感受到的场。若每个电子和离子都作为独立电荷源，计算量会随粒子数快速增加。当前代码用 y-bin 合并 charged rings 来降低开销，但在高增益、高事例数或强空间电荷条件下仍可能较慢。

### 19.5 如何加快运行？

可以考虑：

- 降低 `nEvents`；
- 关闭空间电荷作为基准对照；
- 增大 `spaceChargeBinWidth`，减少 charged-ring 数量；
- 降低电压或压强组合，避免雪崩过大；
- 降低 `maxTotalIonisations`，但要注意这会截断雪崩；
- 关闭不必要的图像输出；
- 批量扫描时先用小事例数粗扫，再对感兴趣点增加统计量。

## 20. 建议的分析流程

推荐按以下顺序分析输出：

1. 先读 `run_summary`，确认参数是否正确。
2. 再读 `avalanche_events`，检查每个事例的增益、吸附和硬停止标记。
3. 若有硬停止事例，先判断这些事例是否应从终态扩散分析中剔除。
4. 用 `endpoint_fit_summary` 读取末态高斯拟合扩散。
5. 用 `electron_diffusion` 分析扩散随时间变化。
6. 用 `endpoint_distributions` 和 `diffusion` 图检查分布形状是否合理。
7. 做多参数扫描时，以文件名和 `run_summary` 双重确认参数，避免混淆。

## 21. 当前默认常量汇总

| 常量 | 当前值 | 含义 |
| --- | --- | --- |
| `nEvents` | `3` | 默认事例数 |
| `gapUm` | `215` | 默认气隙厚度 |
| `pressureAtm` | `1.0` | 默认压强 |
| `voltage` | `-750 V` | 默认上极板电压 |
| `magFieldY` | `0.0 T` | 默认 y 方向磁场 |
| `enableSpaceCharge` | `true` | 默认开启空间电荷 |
| `spaceChargeBinWidth` | `2.e-4 cm` | 空间电荷 y-bin 宽度，即 2 um |
| `avalancheSizeLimit` | `5000` | Garfield++ 内部雪崩规模限制 |
| `maxTotalIonisations` | `50000` | 代码层面总电离数硬停止阈值 |
| `collectionTolerance` | `5.e-5 cm` | 下极板收集判据容差，即 0.5 um |
| `e0` | `0.1 eV` | 初始电子能量 |
| `plotField` | `true` | 默认保存电场图 |
| `plotDrift` | `false` | 默认不保存漂移轨迹图 |

## 22. 逐碰撞能量、动量和分子来源统计

### 22.1 Garfield++ 接口扩展

标准 Garfield++ 2025.12 的碰撞回调没有稳定电子轨迹编号，也不能把跨时间窗口的自由飞行可靠地关联起来。本项目对本地 Garfield++ 做了小范围扩展：

- 每个电子具有稳定的 `trackId`；
- 次级电子保存 `parentTrackId`；
- 轨迹编号和未完成的电场冲量会跨 `ResumeAvalanche()` 时间窗口保留；
- 碰撞回调返回本次碰撞前、后的电子方向；
- 回调返回从上次真实碰撞到本次真实碰撞之间累计的电场冲量。

修改和安装位置为：

```text
/home/yzhao/software/garfieldpp/src-2025.12
/home/yzhao/software/garfieldpp/install-2025.12
```

原始文件备份在：

```text
/home/yzhao/software/garfieldpp/backups/2026-06-09-collision-statistics
```

因此，本项目当前源码必须与这个扩展后的 Garfield++ 一起使用。若重新安装官方未修改版本，`SetUserHandleCollision` 的函数签名会不匹配，主程序无法编译。

### 22.2 每次碰撞能量

程序通过 `SetUserHandleCollision` 记录每一次真实电子碰撞。Null collision 不写入统计。

每次碰撞同时保存：

```text
energyBefore
energyAfter
```

单位均为 eV。默认碰撞能谱使用 `energyBefore`，表示电子真正发生该次碰撞时的入射动能。

对应 ROOT 对象包括：

- `collision_energy_before`：全部真实碰撞的入射能量；
- `collision_energy_elastic`：弹性碰撞；
- `collision_energy_ionisation`：电离碰撞；
- `collision_energy_attachment`：吸附碰撞；
- `collision_energy_inelastic`：非弹性碰撞；
- `collision_energy_excitation`：激发碰撞；
- `collision_energy_superelastic`：超弹性碰撞。

### 22.3 电子动量计算

由电子动能计算相对论动量大小：

```text
p(E) = sqrt(E * (E + 2 m_e c^2))
```

其中：

```text
m_e c^2 = 510998.95 eV
```

动量单位为 `eV/c`。程序在碰撞回调中临时结合 Garfield++ 返回的方向计算矢量动量，用于得到总动量变化；方向和动量分量不写入 TTree。

### 22.4 电场冲量

电场给电子的冲量定义为：

```text
Delta p_E = q integral(E dt)
```

程序在 Garfield++ 内部对每个已接受的自由飞行段进行累计。每段使用轨迹中点处的总电场进行数值积分：

```text
Delta p_E ~= q E(midpoint) Delta t
```

总电场来自 `Sensor::ElectricField`，因此同时包含：

- 平行板外加电场；
- 开启空间电荷时的 charged-ring 电场。

最终只保存冲量大小分布：

```text
field_impulse_magnitude
```

它是 ROOT 直方图，不保存每次自由飞行的冲量数据。单位为 `eV/c`。

这是对严格积分的逐飞行段数值近似。空间电荷场变化很快时，可进一步将一次自由飞行细分为更多积分段以提高精度。

### 22.5 相邻碰撞机械动量变化

对于同一个 `trackId`，程序保存上一次碰撞后的机械动量，并在下一次碰撞前计算：

```text
Delta p_mechanical = p_before(current) - p_after(previous)
```

程序只保留矢量差的模：

```text
free_flight_delta_p_magnitude
```

它是 ROOT 直方图，不保存三个分量或逐次自由飞行数据，单位为 `eV/c`。每条电子轨迹的第一次碰撞没有前一碰撞，因此不会填入该直方图。

无磁场且电场积分足够精确时，机械动量变化应接近电场冲量。在磁场中，机械动量变化还包含洛伦兹力造成的方向变化，因此它不应被解释为纯电场贡献。

### 22.6 吸附电子能量和来源

吸附碰撞由：

```text
type == ElectronCollisionTypeAttachment
```

识别。

默认吸附能谱为：

```text
attachment_energy_before
```

即吸附发生前的电子动能。程序也保留：

```text
attachment_energy_after
```

便于检查 Garfield++ 对吸附碰撞返回的末态能量。

吸附分子和具体过程由 `MediumMagboltz::GetLevel` 解析。例如当前混合气体测试中，吸附过程可以被识别为 SF6 的 attachment 截面项。

### 22.7 雪崩电子产生来源

`electron_birth_sources` 每行对应一个由雪崩新产生的电子，不包括最初注入的初始电子。字段包括：

| 分支 | 含义 |
| --- | --- |
| `event` | 事例编号 |
| `type` | 产生该电子的碰撞类型 |
| `level` | Magboltz 截面能级编号 |
| `gasIndex` | 混合气体组分编号 |
| `gasName` | 分子名称 |
| `process` | Magboltz 过程描述 |
| `isPenning` | 是否由激发后的 Penning 转移产生 |

直接电离一般具有：

```text
type = ElectronCollisionTypeIonisation
isPenning = 0
```

Penning 电子具有：

```text
type = ElectronCollisionTypeExcitation
isPenning = 1
```

### 22.8 `electron_collisions` TTree

该 TTree 每次真实碰撞一行，主要字段如下：

| 分支 | 含义 |
| --- | --- |
| `event` | 事例编号 |
| `trackId` | 当前电子稳定轨迹编号 |
| `parentTrackId` | 产生当前电子的母电子轨迹编号 |
| `hasParent` | 是否具有母轨迹 |
| `collisionIndex` | 当前电子内部的碰撞序号，从 0 开始 |
| `type`, `level` | Garfield++ 碰撞类型和 Magboltz 能级 |
| `gasIndex`, `gasName` | 碰撞气体组分 |
| `process` | Magboltz 碰撞过程描述 |
| `thresholdEnergy` | 该截面过程的阈值能量，单位 eV |
| `energyBefore`, `energyAfter` | 碰撞前后动能，单位 eV |
| `isIonisation` | 是否为直接电离碰撞 |
| `isAttachment` | 是否为吸附碰撞 |
| `isPenning` | 该激发碰撞是否产生 Penning 电子 |

### 22.9 来源汇总和图像

`collision_source_summary` 按 `gasName + process` 汇总：

- 所有碰撞数；
- 产生的雪崩电子数；
- Penning 电子数；
- 吸附电子数。

图像对象包括：

- `collision_statistics` 和 `collision_statistics_png`；
- `collision_sources` 和 `collision_sources_png`；
- `ionisation_electron_sources`；
- `attachment_sources`；
- `field_impulse_magnitude`；
- `free_flight_delta_p_magnitude`；
- `collision_position_x/y/z`；
- `collision_time`；
- `collision_position_time_statistics` 和 `collision_position_time_statistics_png`。

### 22.10 ROOT 分析示例

查看前 20 次碰撞：

```cpp
electron_collisions->Scan(
    "event:trackId:collisionIndex:type:gasName:energyBefore");
```

只查看吸附碰撞：

```cpp
electron_collisions->Scan(
    "event:trackId:gasName:process:energyBefore",
    "isAttachment == 1");
```

查看相邻碰撞总动量变化统计：

```cpp
free_flight_delta_p_magnitude->Draw();
```

查看电场冲量大小统计：

```cpp
field_impulse_magnitude->Draw();
```

查看雪崩电子由哪些分子产生：

```cpp
electron_birth_sources->Scan("gasName:process:isPenning");
```

查看来源汇总：

```cpp
collision_source_summary->Scan();
```

### 22.11 数据量和性能

`electron_collisions` 仍保存每次真实碰撞的能量、类型、轨迹编号和分子来源，但不再保存位置、时间、方向、动量分量、电场冲量和自由飞行动量变化的逐条数据。这些运动学量仅保存为汇总直方图和 PNG 图像，从而显著降低 ROOT 文件大小。

高增益事例仍可能产生大量能量和来源记录。若文件依然过大，可进一步用 `level` 映射表替代逐行重复的 `gasName` 和 `process` 字符串。
