# 215 μm、1 atm、1900 V、0 T、空间电荷开启时的 ROOT 文件逐项物理分析

## 1. 分析对象与方法

分析文件：

```text
/ustcfs/STCFUser/yzhao/Simulation/Garfield/MySim/Magnetic/MicroscopicSpaceCharge/build_RPCgas_90_5_5/215um_1.00atm_1900V_0.00T_1e_SC1.root
```

分析日期：2026-06-10。

文件大小为 14,212,174 byte，约 13.55 MiB，ROOT 压缩因子约 5.05。文件含 61 个顶层对象，`diffusion_event_graphs` 子目录内另有 1 个事件图。本文直接读取每个直方图、TTree、拟合函数、图和画布的数值；分位数来自 ROOT 直方图，物理解释结合程序实现和 Garfield++ 状态定义。

需要先说明：目录名 `build_RPCgas_90_5_5` 暗示某种 90/5/5 配比，但 ROOT 的 `run_summary` 没有保存气体配比。因此本文能确认气体组分为 C2H2F4、iC4H10、SF6，却不能只靠此文件严格证明其体积分数。后续应把气体名称和配比写入 `run_summary`。

## 2. 运行条件与总体结果

`run_summary` 给出的条件如下：

| 参数 | 数值 | 解释 |
|---|---:|---|
| 事例数 | 1 | 所有空间偏心和涨落只能视为单事例结果 |
| 气隙 | 215 μm | `0.0215 cm` |
| 电压 | -1900 V | 电子由上电极附近向 `y=0` 电极漂移 |
| 名义平均场强 | 88.37 kV/cm | 不含空间电荷畸变时的 `1900 V / 215 μm` |
| 约化场强 | 约 353 Td | 按 293.15 K、1 atm 的理想气体数密度估算 |
| 磁场 | 0 T | 不存在洛伦兹力导致的磁偏转 |
| 空间电荷 | 开启 | 最终电场不再严格均匀 |
| 空间电荷 bin 宽 | 2 μm | 215 μm 气隙使用 108 个 bin，最后一个覆盖到略高于气隙边界 |
| 初始位置 | `(0, 214.9 μm, 0)` | 距上电极约 0.1 μm |
| 初始能量 | 0.1 eV | 低能种子电子 |
| 同时存在的雪崩电子上限 | 100,000 | 扩散树中峰值 99,999，说明上限实际起作用 |
| 总电离硬限制 | 1,000,000 | 本事例 661,180，未触发 |

`avalanche_events` 与 `run_summary` 一致：

- 电离回调累计数：661,180；
- 程序定义的增益：661,181，即初始电子加全部电离次数；
- 吸附数：46,103；
- 到达收集电极的电子：234,257；
- `stoppedByIonisationLimit=0`，不是由总电离硬限制终止。

这里必须区分“程序增益”和“实际收集电荷”。程序的 `gain=1+ionisations` 是毛电离计数。由于同时存在的雪崩电子被限制在 100,000，达到限制后仍可能发生并统计电离碰撞，但次级电子不一定全部继续作为独立电子被运输。因此 661,181 不能直接当作收集增益。实际收集电子数为 234,257，约为该毛增益的 35.43%。做探测器有效增益研究时，应优先使用收集电子数或感应电荷，而不是当前 `gain` 字段。

## 3. 碰撞能量对象

### 3.1 `collision_energy_before`

共 423,271,514 次真实碰撞。碰撞前能量均值 8.366 eV，RMS 4.763 eV，中位数 7.727 eV，5% 到 95% 区间约 1.711–17.305 eV，最高有内容的区域延伸到约 128 eV。

分布主体位于数 eV 到十几 eV，是电子在强电场中加速与频繁碰撞损能达到的非平衡能量分布。长高能尾负责跨越激发和电离阈值，但大部分电子碰撞能量低于典型电离阈值，所以电离只占全部碰撞的很小比例。

### 3.2 `collision_energy_after`

条目数相同，均值 8.205 eV，RMS 4.673 eV，中位数 7.595 eV。它相对碰撞前能量整体略低，平均降低约 0.161 eV。这符合碰撞将电子能量转移给分子的物理过程；由于 82.7% 的碰撞是弹性碰撞，而且大量非弹性过程是低能振动激发，所以总体平均损失不大。

### 3.3 `attachment_energy_before` 与 `attachment_energy_after`

均有 46,103 条，均值约 2.203 eV，中位数约 0.408 eV，95% 分位约 10.59 eV。吸附主要发生在低能区，这是 SF6 等强电负性分子的典型特征：低能电子容易形成暂态负离子并发生解离或稳定吸附。

前后能量几乎完全相同，不应解释为吸附“没有能量交换”。在 Garfield++ 回调语义中，吸附后电子轨迹立即终止，返回的末态电子能量不一定代表一个可继续传播的自由电子能量，因此 `attachment_energy_after` 的物理信息有限，真正有意义的是吸附发生前能量。

#### 约 5.2 eV 的峰结构

`attachment_energy_before` 在约 5.2 eV 附近出现的峰，主要来自 SF6 的解离电子吸附共振：

```text
e- + SF6 -> SF5 + F-
```

本模拟所使用的 Magboltz SF6 数据将该过程记录为 `ATTACHMENT T=300K F-`。对应截面在约 4.5 eV 开始快速增大，约 5.0--5.5 eV 达到显著峰值，随后随能量升高而下降。因此电子能量进入这一共振区间时，形成暂态 `SF6-*` 并解离为 `SF5 + F-` 的概率显著增加，最终在吸附前能量谱上形成约 5.2 eV 的峰。

ROOT 文件中的来源计数支持这一归属：全部 46,103 次吸附中，SF6 贡献 45,407 次，占 98.49%；其中 `F-` 通道有 11,620 次。接近零能量的强结构则主要还受到 `SF6-`、`SF5-` 等低能吸附通道贡献。

需要注意，吸附能谱不是吸附截面的直接复制。单位能量区间内的吸附计数近似满足：

```text
N_attachment(E) proportional to f_e(E) * v(E) * sigma_attachment(E)
```

其中 `f_e(E)` 是雪崩电子能量分布，`v(E)` 是电子速度，`sigma_attachment(E)` 是各吸附通道截面。因此谱峰位置、宽度和相对高度同时受 SF6 截面共振、电子能量分布、气体配比和直方图 bin 宽影响，峰值不要求与截面表中的单个采样能量完全重合。

### 3.4 按碰撞类型的能谱

| 对象 | 次数 | 占全部碰撞 | 典型能量特征与原因 |
|---|---:|---:|---|
| `collision_energy_elastic` | 350,211,404 | 82.739% | 均值 8.509 eV；弹性截面大且无阈值，因此支配碰撞次数 |
| `collision_energy_inelastic` | 68,770,608 | 16.247% | 均值 7.065 eV；以低阈值振动、转动过程为主 |
| `collision_energy_excitation` | 3,343,284 | 0.790% | 均值 17.778 eV；必须越过约 7–20 eV 的激发阈值 |
| `collision_energy_ionisation` | 661,180 | 0.156% | 均值 21.100 eV；最低内容从约 10.67 eV 开始，对应最低电离阈值 |
| `collision_energy_superelastic` | 238,935 | 0.056% | 均值 6.762 eV；电子从已激发分子获得能量，概率较低 |
| `collision_energy_attachment` | 46,103 | 0.0109% | 低能峰显著；虽占比很小，但会直接移除电子 |

电离能谱在阈值以上出现并具有高能尾，是阈值过程的直接表现。激发能谱同样偏高。非弹性能谱低于激发谱，是因为程序把大量低能振动损失也归入非弹性碰撞。

### 3.5 `collision_statistics`、`collision_statistics_png`

这两个对象分别是汇总画布和同一画布的嵌入 PNG，组合显示碰撞前后能量、吸附前后能量、电场冲量和机械动量变化。它们不包含独立于底层直方图的新物理数据。

### 3.6 `collision_energy_by_type`、`collision_energy_by_type_png`

分别为六类碰撞能谱的画布和 PNG 副本。不同过程的起始阈值和峰位来自各分子的 Magboltz 截面；画布用于比较，定量值应读取对应 TH1D。


## 4. 相邻碰撞之间的动量与空间位移

### 4.1 `field_impulse_magnitude`

共有 423,271,514 条，均值 309.41 eV/c，中位数 201.47 eV/c，95% 分位 985.02 eV/c，分布具有明显右长尾。自由飞行时间近似指数分布，少数较长自由程会积累较大电场冲量，因此模长分布天然右偏。

### 4.2 `free_flight_delta_p_magnitude`

共有 422,991,849 条，比碰撞总数少 279,665 条。这一差值主要来自每条电子轨迹的第一次碰撞：没有“上一次碰撞”可用于计算机械动量变化。均值 309.27 eV/c、中位数 201.40 eV/c，与电场冲量高度一致。

本事例磁场为 0，因此相邻碰撞间电子机械动量变化应主要来自电场力。两个分布几乎相同，是冲量累计和轨迹关联基本自洽的重要检查。微小差异来自数值积分、碰撞瞬间定义和空间电荷场在离散时间步中的更新。

### 4.3 `inter_collision_delta_x`

均值约 `-6.9×10^-10 cm`，可视为 0；RMS `1.388×10^-5 cm`，即 0.139 μm。分布关于 0 近似对称，因为没有 X 方向外加场或磁场，正负位移只由随机散射决定。

### 4.4 `inter_collision_delta_y`

均值 `-1.862×10^-6 cm`，即每次自由飞行平均沿 Y 方向前进约 -0.0186 μm；RMS 0.139 μm。负均值来自电场驱动电子从 `y≈215 μm` 向 `y=0` 运动。随机热运动和散射宽度远大于单次平均漂移，所以分布仍跨越正负两侧。

### 4.5 `inter_collision_delta_z`

均值约 `9.7×10^-10 cm`，可视为 0；RMS `1.387×10^-5 cm`。它与 X 分布几乎相同，符合 0 T 且横向几何对称时的横向各向同性。

### 4.6 `inter_collision_abs_delta_x/y/z`

三个方向平均绝对位移分别为 0.08486、0.08547、0.08485 μm，中位数约 0.0471–0.0474 μm。Y 方向只略大于 X/Z，说明单次自由飞行主要由随机方向散射控制，宏观漂移是大量微小负 Y 偏置累积形成的。

### 4.7 `inter_collision_distance`

两碰撞点间直线距离均值 `1.702×10^-5 cm`，即 0.170 μm；中位数 0.118 μm，95% 分位 0.513 μm。右长尾来自自由程的随机性。该量是碰撞点间弦长，不是电场中弯曲轨迹的严格弧长；本事例 B=0 时二者差异通常较小，但仍可能受非均匀电场影响。

### 4.8 两组位移画布

- `inter_collision_signed_displacement` 与 `inter_collision_signed_displacement_png`：显示 Δx、Δy、Δz；
- `inter_collision_distance_statistics` 与 `inter_collision_distance_png`：显示三个绝对分量和总距离。

它们是上述七个直方图的可视化副本，不是额外统计样本。

## 5. 碰撞位置与时间

### 5.1 `collision_position_x`

均值 0.724 μm，RMS 13.10 μm，90% 中央区间约 -20.26 至 22.32 μm。横向展宽来自多次随机散射。均值没有严格落在 0，是单个雪崩的统计偏心以及空间电荷反馈共同造成的；只有一个事例，不能把 0.724 μm 解释为系统性 X 漂移。

### 5.2 `collision_position_y`

均值 31.55 μm，中位数 27.33 μm，95% 的碰撞位置低于约 75.29 μm，而气隙上边界为 215 μm。碰撞强烈集中在收集电极附近，原因是雪崩电子数沿漂移方向指数增长：靠近 `y=0` 时电子群最大，因此即使单个电子在整个气隙都有碰撞，按全部电子加权后的碰撞总数仍由末段支配。

### 5.3 `collision_position_z`

均值 -8.052 μm，RMS 13.90 μm。Z 方向没有外加驱动力，这个非零中心主要是单事例雪崩簇随机向负 Z 侧发展的结果，并被后续倍增和空间电荷反馈放大。末态电子 Z 均值也约 -8.14 μm，与该解释一致。

### 5.4 `collision_time`

均值 1.058 ns，RMS 0.175 ns，中位数 1.052 ns，主要范围约 0.789–1.346 ns。早期电子数很少，虽然已经发生碰撞，但对总计数贡献低；约 0.8–1.3 ns 时雪崩电子数达到数万至十万，因而碰撞时间分布在这一阶段形成主峰。约 1.7 ns 后电子全部终止。

### 5.5 `collision_position_time_statistics` 与 PNG

`collision_position_time_statistics` 和 `collision_position_time_statistics_png` 只是 X/Y/Z/时间四个直方图的组合显示。

## 6. 分子与碰撞过程来源

### 6.1 `collision_source_summary`

该 TTree 有 96 行，每行对应一个 `气体 | Magboltz过程`。按气体汇总：

| 气体 | 碰撞数 | 碰撞占比 | 电离电子 | 电离占比 | 吸附 | 吸附占比 |
|---|---:|---:|---:|---:|---:|---:|
| C2H2F4 | 357,759,363 | 84.52% | 443,923 | 67.14% | 696 | 1.51% |
| iC4H10 | 40,581,568 | 9.59% | 201,619 | 30.49% | 0 | 0 |
| SF6 | 24,930,583 | 5.89% | 15,638 | 2.37% | 45,407 | 98.49% |

碰撞比例不等于气体体积分数，因为它还乘入各组分随能量变化的截面。C2H2F4 弹性及振动截面贡献很大，因此占绝大多数碰撞。iC4H10 的最低电离阈值约 10.67 eV，虽然碰撞占比不高，却贡献约 30.5% 电离。SF6 是强电负性气体，电离贡献小，却吸收约 98.5% 的附着电子。

最主要碰撞道包括：C2H2F4 弹性 293,600,097 次、iC4H10 弹性 35,822,841 次、C2H2F4 多个振动模合计数千万次、SF6 弹性 20,788,466 次。大量低能弹性和振动碰撞解释了总碰撞数极大而电离概率仅 0.156%。

### 6.2 `electron_birth_sources`

该 TTree 有 661,180 行，与电离回调数完全一致。`type` 全为直接电离，`isPenning` 全为 0。主要电离来源为：

- C2H2F4 电离：443,923；
- iC4H10 普通电离：130,530；
- iC4H10 电离-激发/碎裂：71,089；
- SF6 各碎片离子通道合计：15,638。

没有 Penning 电子并不表示气体中绝对不存在 Penning 物理，而是本次所用 Magboltz/Garfield++ 配置未产生已实现的 Penning 转移记录。

### 6.3 `collision_sources_all`

这是将 96 个来源的碰撞总数写入带标签直方图。其 ROOT `entries=96` 只是设置了 96 个分类 bin，不是只有 96 次碰撞；真实总数是各 bin 内容之和 423,271,514。分类直方图的均值/RMS没有连续物理坐标意义。

### 6.4 `ionisation_electron_sources`

各分类 bin 内容之和为 661,180。只有电离过程 bin 非零，峰值是 C2H2F4 电离，其次是 iC4H10 的两个电离通道。

### 6.5 `attachment_sources`

各 bin 之和为 46,103。主通道为：SF6- 26,913、F- 11,620、SF5- 5,753、SF4- 836；C2H2F4 二体和三体吸附合计 696。该分布直接表现了 SF6 对雪崩淬灭和电子捕获的主导作用。

### 6.6 `collision_sources` 与 PNG

`collision_sources` 和 `collision_sources_png` 将总碰撞来源、电离来源、吸附来源并排显示。由于不同过程跨越八个数量级，线性坐标下小通道可能难以辨认；精确比较应读取 `collision_source_summary`。


## 7. 末态电子与收集电子分布

### 7.1 `electron_endpoints`

该 TTree 有 280,360 行，是文件中最大的对象，压缩后约 8.57 MB。只有两种状态：

| Garfield++ 状态 | 行数 | 物理含义 | 平均 Y | 平均时间 |
|---:|---:|---|---:|---:|
| -5 | 234,257 | `StatusLeftDriftMedium`，在本程序中接近 `y=0` 者视为收集电子 | `8.63×10^-10 cm` | 1.2805 ns |
| -7 | 46,103 | `StatusAttached`，电子被分子吸附 | 31.83 μm | 1.0616 ns |

两者之和正好是 280,360。吸附电子平均在更早时间、更高 Y 位置终止；这是因为它们在到达收集电极前被 SF6 等分子捕获。收集电子集中在数值上接近 0 的 Y 坐标。

端点数与毛电离增益不守恒，主要原因是雪崩并发尺寸上限为 100,000。该限制会使“电离碰撞累计数”和“实际继续运输的电子数”脱钩，因此不能用 `1 + 电离 - 吸附` 直接预测端点数。

### 7.2 `endpoint_x`

收集电子 X 均值 0.343 μm，RMS 14.984 μm；5%–95% 区间约 -23.71 至 24.69 μm。它近似以零为中心，宽度反映横向扩散和空间电荷造成的横向展宽。

### 7.3 `endpoint_y`

均值 `8.63×10^-10 cm`，即约 0.0086 nm；这不是纵向扩散宽度，而是粒子跨越 `y=0` 边界时数值定位的残余。所有收集电子都必须落在同一电极面附近，因此分布高度非高斯且尺度由边界查找精度决定。

### 7.4 `endpoint_z`

均值 -8.136 μm，RMS 15.535 μm；5%–95% 区间约 -33.21 至 16.92 μm。宽度与 X 相近，符合横向扩散近似各向同性；中心偏移来自单事例雪崩簇的随机偏心和空间电荷反馈，不能作为 0 T 下系统性 Z 漂移的证据。

### 7.5 `endpoint_time`

均值 1.2805 ns，RMS 0.1034 ns；5%–95% 区间 1.106–1.447 ns。到达时间宽度来自初始随机碰撞历史、纵向扩散、雪崩中电子的不同出生位置以及空间电荷造成的局部漂移速度变化。

### 7.6 四个高斯拟合函数

- `fit_endpoint_x`：拟合成功，均值 0.336 μm，σ=15.028 μm；
- `fit_endpoint_z`：拟合成功，均值 -8.147 μm，σ=15.578 μm；
- `fit_endpoint_time`：拟合成功，均值 1.28028 ns，σ=0.10364 ns；
- `fit_endpoint_y`：拟合状态为 4，失败，参数不应使用。

X、Z、时间拟合均在原分布均值的正负 3 RMS 范围内进行。虽然 ROOT 返回状态 0，但 `χ²/ndf` 分别约 32、49 和 15，远大于 1。样本量非常大时，轻微非高斯尾、空间电荷畸变和 binning 都会产生很大 χ²。因此这些 σ 应理解为核心区的“有效高斯宽度”，不是完整分布严格服从高斯的证明。

Y 拟合失败完全符合物理预期：端点被电极边界钉在 `y≈0`，其分布不是扩散高斯。`fit_endpoint_y` 对象虽然被写入文件，但 `endpoint_fit_summary` 已用 `statusY=4`、`meanY=-1`、`sigmaY=-1` 标记无效。直接查看 TF1 内部残留参数会得到不合理值，必须忽略。

### 7.7 `endpoint_fit_summary`

该 TTree 汇总拟合状态和参数。有效的横向综合宽度：

```text
sigmaTransverse = sqrt((sigmaX² + sigmaZ²)/2) = 15.305 μm
```

这与时间演化曲线在电子数充足阶段的最大横向宽度约 15.8 μm 接近，说明末态拟合与动态扩散统计在量级上自洽。

### 7.8 `endpoint_distributions` 与 PNG

这两个对象分别为端点 X/Y/Z/时间画布和嵌入 PNG。画面中的 Y 拟合曲线即使被绘制，也不代表拟合有效，必须结合 `endpoint_fit_summary.statusY` 判断。

## 8. 雪崩随时间的扩散

### 8.1 `electron_diffusion`

该 TTree 有 40 帧，保存活电子数、质心和三个方向宽度。主要演化阶段：

1. **0.05 ns：** 只有 1 个电子，所以所有 σ 都是 0。
2. **0.10–0.90 ns：** 电子数从 4 增至 99,999，横向 `sigmaT` 从 2.34 μm 增至 11.97 μm。随机散射不断扩大电子云，同时电离倍增迅速增强统计量。
3. **约 0.90–1.00 ns：** 活电子数贴近 100,000，说明 `avalancheSizeLimit` 主导并发规模。
4. **1.00–1.45 ns：** 大量电子到达电极或被吸附，活电子数下降；横向宽度仍增至最大约 15.85 μm。
5. **1.45 ns 后：** 剩余电子数从 7,182 快速降到 7，`sigmaT` 随后下降；这是宽分布中的电子先终止、仅少数幸存电子被统计造成的幸存者偏差和小样本涨落，不是物理上的“反扩散”。
6. **1.719 ns：** 电子数为 0，最后两帧 σ=0 是空集合占位值，不能用于扩散拟合。

质心 Y 从 209.3 μm 单调向电极移动。X/Z 质心在后期出现较大偏移，是活电子数很少时的统计涨落以及单雪崩空间电荷不对称造成的。

`sigmaT` 定义为：

```text
sqrt((sigmaX² + sigmaZ²) / 2)
```

它是横向几何宽度，不是直接的扩散系数。若要得到扩散系数，应在空间电荷影响较弱、电子数足够且未受边界截断的时间区间拟合 `sigmaT²` 对时间或漂移距离的斜率。

### 8.2 `diffusion_event_graphs/diffusion_sigma_t_event_0`

这是事例 0 的 40 点 `sigmaT(t)` 图，逐点内容与 `electron_diffusion` 一致。将每个事例图放入子目录可以避免大量事例时 ROOT 顶层目录拥挤。

### 8.3 `diffusion_graph`

该 TMultiGraph 包含 1 条事件曲线，因为本文件只有 1 个事例。多事例运行时它会叠加各事例曲线用于比较。

### 8.4 `diffusion` 与 `diffusion_png`

分别为扩散总画布和 PNG 副本。曲线后期回落必须结合 `nElectrons` 解读，不能只看图形外观。

## 9. 电场对象

### 9.1 `field`

这是 X-Y 平面上的电场强度彩色图，绘制时使用总 `Sensor`，包含平行板外场以及开启空间电荷后 charged-ring 模型产生的场。名义平均场为 88.37 kV/cm，但雪崩电子、正离子和负离子会在局部增强或屏蔽电场。

该 ROOT 文件只保存画布，没有保存规则网格上的电场数值，因此可以定性观察场畸变，却不能从文件中重新做精确场剖面或计算局部最大场。若要定量研究空间电荷畸变，应额外保存 `E(x,y)` 或至少中心轴 `E_y(y)` 的 TGraph/TH2D。

该图在模拟结束阶段生成，因而更接近最终空间电荷配置的快照，而不是雪崩全过程的时间平均场。

### 9.2 `field_png`

它是 `field` 画布的嵌入 PNG，不含独立数值。


## 10. 全部对象索引

下表覆盖 61 个顶层对象及子目录内的事件图。标为“画布/PNG”的对象只是前述数据的显示形式。

| 序号 | ROOT 对象 | 类型 | 物理内容或作用 |
|---:|---|---|---|
| 1 | `collision_energy_before` | TH1D | 全部真实碰撞前电子能量 |
| 2 | `collision_energy_after` | TH1D | 全部真实碰撞后电子能量 |
| 3 | `attachment_energy_before` | TH1D | 吸附发生前能量，主要物理量 |
| 4 | `attachment_energy_after` | TH1D | 回调返回的吸附后能量，物理意义有限 |
| 5 | `field_impulse_magnitude` | TH1D | 两碰撞间电场冲量模 |
| 6 | `free_flight_delta_p_magnitude` | TH1D | 两碰撞间机械动量变化模 |
| 7 | `inter_collision_delta_x` | TH1D | 带符号 Δx |
| 8 | `inter_collision_delta_y` | TH1D | 带符号 Δy，负均值表现电子漂移方向 |
| 9 | `inter_collision_delta_z` | TH1D | 带符号 Δz |
| 10 | `inter_collision_abs_delta_x` | TH1D | `|Δx|` |
| 11 | `inter_collision_abs_delta_y` | TH1D | `|Δy|` |
| 12 | `inter_collision_abs_delta_z` | TH1D | `|Δz|` |
| 13 | `inter_collision_distance` | TH1D | 两碰撞点三维直线距离 |
| 14 | `collision_position_x` | TH1D | 全部碰撞的 X 位置 |
| 15 | `collision_position_y` | TH1D | 全部碰撞的 Y 位置，集中在收集电极附近 |
| 16 | `collision_position_z` | TH1D | 全部碰撞的 Z 位置 |
| 17 | `collision_time` | TH1D | 全部碰撞时间 |
| 18 | `collision_energy_elastic` | TH1D | 弹性碰撞能谱 |
| 19 | `collision_energy_ionisation` | TH1D | 电离碰撞能谱 |
| 20 | `collision_energy_attachment` | TH1D | 吸附碰撞能谱 |
| 21 | `collision_energy_inelastic` | TH1D | 非弹性碰撞能谱，含低能振动过程 |
| 22 | `collision_energy_excitation` | TH1D | 电子激发碰撞能谱 |
| 23 | `collision_energy_superelastic` | TH1D | 超弹性碰撞能谱 |
| 24 | `electron_birth_sources` | TTree | 每个电离回调产生电子的分子和过程来源 |
| 25 | `collision_source_summary` | TTree | 96 个气体-过程通道的汇总计数 |
| 26 | `collision_sources_all` | TH1D | 全碰撞来源分类图 |
| 27 | `ionisation_electron_sources` | TH1D | 电离电子来源分类图 |
| 28 | `attachment_sources` | TH1D | 吸附来源分类图 |
| 29 | `collision_statistics` | TCanvas | 主要能量和动量统计画布 |
| 30 | `collision_statistics_png` | TASImage | 上述画布 PNG |
| 31 | `collision_energy_by_type` | TCanvas | 六类碰撞能谱画布 |
| 32 | `collision_energy_by_type_png` | TASImage | 上述画布 PNG |
| 33 | `inter_collision_signed_displacement` | TCanvas | Δx、Δy、Δz 画布 |
| 34 | `inter_collision_signed_displacement_png` | TASImage | 上述画布 PNG |
| 35 | `inter_collision_distance_statistics` | TCanvas | 三个绝对位移及总距离画布 |
| 36 | `inter_collision_distance_png` | TASImage | 上述画布 PNG |
| 37 | `collision_position_time_statistics` | TCanvas | 碰撞 X/Y/Z/时间画布 |
| 38 | `collision_position_time_statistics_png` | TASImage | 上述画布 PNG |
| 39 | `collision_sources` | TCanvas | 碰撞、电离、吸附来源画布 |
| 40 | `collision_sources_png` | TASImage | 上述画布 PNG |
| 41 | `run_summary` | TTree | 运行配置和全局总数 |
| 42 | `avalanche_events` | TTree | 每事例增益、电离、吸附、收集数 |
| 43 | `electron_endpoints` | TTree | 所有终止电子的状态、坐标和时间 |
| 44 | `endpoint_x` | TH1D | 收集电子末态 X |
| 45 | `endpoint_y` | TH1D | 收集电子电极边界残余 Y |
| 46 | `endpoint_z` | TH1D | 收集电子末态 Z |
| 47 | `endpoint_time` | TH1D | 收集电子到达时间 |
| 48 | `fit_endpoint_x` | TF1 | X 分布 ±3σ 高斯拟合 |
| 49 | `fit_endpoint_y` | TF1 | Y 拟合对象，但拟合失败，不可使用 |
| 50 | `fit_endpoint_z` | TF1 | Z 分布 ±3σ 高斯拟合 |
| 51 | `fit_endpoint_time` | TF1 | 到达时间 ±3σ 高斯拟合 |
| 52 | `endpoint_fit_summary` | TTree | 拟合状态、均值、σ 和区间 |
| 53 | `endpoint_distributions` | TCanvas | 末态坐标和时间画布 |
| 54 | `endpoint_distributions_png` | TASImage | 上述画布 PNG |
| 55 | `electron_diffusion` | TTree | 40 帧活电子质心和扩散宽度 |
| 56 | `diffusion_event_graphs` | TDirectoryFile | 每事例扩散图子目录 |
| 56.1 | `diffusion_sigma_t_event_0` | TGraph | 事例 0 的 `sigmaT(t)` |
| 57 | `diffusion_graph` | TMultiGraph | 所有事例扩散曲线集合，本文件含 1 条 |
| 58 | `diffusion` | TCanvas | 扩散曲线画布 |
| 59 | `diffusion_png` | TASImage | 扩散画布 PNG |
| 60 | `field` | TCanvas | 模拟结束阶段的总电场图 |
| 61 | `field_png` | TASImage | 电场图 PNG |

## 11. 物理结论与使用建议

1. 该场强下电子平均碰撞能量约 8 eV，绝大多数碰撞是弹性或低能振动碰撞；只有约 0.156% 的碰撞产生电离。
2. SF6 虽只贡献约 5.9% 的碰撞，却造成约 98.5% 的吸附，是控制雪崩和降低有效增益的主要电负性组分。
3. 0 T 条件下电场冲量和机械动量变化分布高度一致，说明跨碰撞轨迹关联和冲量统计基本合理。
4. 横向单步位移关于 0 对称，Y 单步位移具有负偏置，宏观漂移由大量随机自由飞行上的微小方向偏置累积而成。
5. 收集电子横向有效高斯宽度约 15.3 μm，但 X/Z 拟合的 χ²/ndf 很大，应把它视为核心宽度；不能宣称完整分布严格高斯。
6. Y 端点拟合失败是正确结果，因为收集电子被边界钉在 `y=0`，Y 分布不是扩散分布。
7. 扩散曲线约在 1.45 ns 达到 15.85 μm，之后下降由电子数骤减和边界选择造成，不能解释为扩散逆转。
8. 当前 `gain=661181` 是毛电离回调计数，并受雪崩并发尺寸限制影响；与收集电子 234,257 不是同一物理量。比较实验有效增益时应使用收集电荷或感应信号。
9. 只有一个事例。X/Z 质心偏移和高增益涨落不能作为稳定结论；研究压强、磁场或空间电荷趋势时应运行多个事例并给出均值、标准差和置信区间。
10. 建议未来在 `run_summary` 增加气体配比、温度、平均场强、约化场强以及是否触发 avalanche size limit；在 ROOT 中保存电场剖面数值，而不只保存画布。
